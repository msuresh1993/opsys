#include<sys/defs.h>
#include<sys/sbunix.h>
#include<sys/pagingglobals.h>
page_t * free_list = NULL;
uint64_t *free_list_location = NULL;
uint64_t MAX_NUMBER_PAGES = 0;
int check_physical_frame(uint64_t current_addr, physical_map_node *test, uint64_t num_physical_regions, uint64_t physbase, uint64_t physfree){
	uint64_t current_index = 0;
	int check = 0;
	for(current_index = 0; current_index <num_physical_regions; current_index++){
		if(test[current_index].start <= current_addr && current_addr+ PAGE_SIZE <=test[current_index].end){
			check = 1;
		}
	}
	if( check == 1 && ((current_addr + PAGE_SIZE)<physbase || physfree < current_addr)){//can do <= but i dont mind wasting a page at the boundary.
		return 1;
	}
	return 0;
}
//marks the address that that you provide as argument. If the address is not physically aligned then it does nothing.
void mark_frame_used(uint64_t address){
	uint64_t index = 0;
	for(; index<MAX_NUMBER_PAGES; index++){
		if(free_list[index].frame_addr == address){
			free_list[index].is_free = 0;
			free_list[index].ref_count = 1;
			break;
		}
	}
}
/*this function has not been completed yet, we will get to it as and when we need it.*/
void create_free_list_test(uint32_t* modulep, page_t *free_list, void *physbase, void *physfree) {
	physical_map_node test[10];//assuming there are not more than 10 disjoint separate memory regions.
	int test_index = 0;
	struct smap_t* smap;
	for (smap = (struct smap_t*) (modulep + 2);
			smap < (struct smap_t*) ((char*) modulep + modulep[1] + 2 * 4);
			++smap) {
		if (smap->type == 1 /* memory */&& smap->length != 0) {
			if(test_index < 10){
				test[test_index].start = smap->base;
				test[test_index].end = smap->base + smap->length;
				test_index++;

			}
		}
	}
	int num_physical_regions = test_index;
	int max_memory_address = test[num_physical_regions - 1].end;
	int min_memory_address = test[0].start;
	//so we have test that has the regions of physical memory and we have physbase and physfree that
	//has the kernel, we need to make sure that the free list does not mark the kernel as free.
	//so the condition is that the memory frame is free if it is in test and is not in the region between
	//physbase and physfree
	//so I will start the linked list from the first aligned page after physfree, go uptill end of memory
	//then start from the beginning and go till the physbase.
	uint64_t start_linked_list = (uint64_t) ((((uint64_t) physfree)
					& (~(PAGE_SIZE - 1))) + (PAGE_SIZE));
	uint64_t current_addr;
	uint64_t index = 0;
	for(current_addr = start_linked_list; current_addr < max_memory_address; index++, current_addr += PAGE_SIZE){
		if ( check_physical_frame(current_addr, test, num_physical_regions, (uint64_t)physbase, (uint64_t)physfree) == 1){
			free_list[index].frame_addr = current_addr;
			free_list[index].is_free = 1; //the page is free
			free_list[index].ref_count = 0;
		}
		else{
			free_list[index].frame_addr = current_addr;
			free_list[index].is_free = 0; //the page is not free
			free_list[index].ref_count = 1;//something is occupying, now kernel
		}

	}
	for(current_addr = min_memory_address; current_addr < start_linked_list; index++, current_addr += PAGE_SIZE){
		if ( check_physical_frame(current_addr, test, num_physical_regions, (uint64_t)physbase, (uint64_t)physfree) == 1){
			free_list[index].frame_addr = current_addr;
			free_list[index].is_free = 1; //the page is free
			free_list[index].ref_count = 0;
		}
		else{
			free_list[index].frame_addr = current_addr;
			free_list[index].is_free = 0; //the page is not free
			free_list[index].ref_count = 1;//something is occupying, now kernel
		}
	}
	MAX_NUMBER_PAGES = index;
}
uint64_t get_free_page(page_t *free_list) {
	return get_free_pages(free_list, 0);
}
//legacy helper functions not needed
int check_boolarray_index(char *array, uint64_t pos, uint64_t limit){
	if(pos>limit)
		return 0;
	uint64_t array_index = pos/8;
	uint64_t array_offset = pos%8;
	if((array[array_index]&(1<<array_offset))!=0)
		return 1;
	return 0;
}
//the start and end should be a valid offset. no check done
int check_boolarray_range(char *array, uint64_t start, uint64_t end, uint64_t limit){
	if(end>limit)
			return 0;
	uint64_t i = start;
	while(i<=end && check_boolarray_index(array, i, limit)){
		i++;
	}
	if(i >end){
		return 1;
	}
	else{
		return 0;
	}
}
void clear_boolarray_index(char *array, uint64_t pos, uint64_t limit){
	if(pos>limit)
		return;
	uint64_t array_index = pos/8;
	uint64_t array_offset = pos%8;
	char MASK = 0xFF ^ 1<<array_offset;
	array[array_index] = array[array_index]&MASK;
}
void clear_boolarray_range(char *array, uint64_t start, uint64_t end, uint64_t limit){
	if(end>limit)//invalid case, so if more bits are asked to be set than allowed then sets nothing
		return;
	uint64_t i = start;
	while(i<=end){
		clear_boolarray_index(array,i,limit);
		i++;
	}

}
//legacy boolarray functions end here
//be careful if you use this function for other stuff
//this function checks 2 things. First, it checks if the range itself is contiguous and also, that the memory addresses it references are contiguous.
int check_array_range(page_t *free_list, uint64_t start, uint64_t end){//no bound check is done.
	uint64_t previous_addr;
	if(free_list[start].is_free == 1){
		previous_addr = free_list[start].frame_addr;
	}
	else{
		return 0;
	}
	for(uint64_t i = start+1; i<=end; i++){
		if(free_list[i].is_free == 1 && free_list[i].frame_addr == (previous_addr+0x1000)){
			previous_addr = free_list[i].frame_addr;
		}
		else{
			return 0;
		}
	}
	return 1;
}
void clear_array_range(page_t *free_list, uint64_t start, uint64_t end){
	for(uint64_t i=start;i<=end;i++){
		free_list[i].is_free = 0;
		free_list[i].ref_count = 1;
	}
}
uint64_t get_free_pages(page_t *free_list,int order){
	uint64_t limit = 1<<order;
	for(uint64_t i = 0; i < MAX_NUMBER_PAGES-limit+1;i++){//todo: optimize
		if(free_list[i].is_free == 1 && free_list[i+limit-1].is_free == 1 && check_array_range(free_list, i, i+limit-1) == 1){
			clear_array_range(free_list,i, i+limit-1);
			uint64_t phys_page = free_list[i].frame_addr;
			return phys_page;
		}
	}
	return 0;
}
void return_page(uint64_t page, page_t *free_list) {
	for(uint64_t i = 0; i <MAX_NUMBER_PAGES; i++){
		if(free_list[i].frame_addr == page){
			free_list[i].ref_count -= 1;
			if(free_list[i].ref_count == 0)
				free_list[i].is_free = 1;
			break;
		}
	}
}

//no checks done, programmer care required.
void return_pages(uint64_t page, page_t *free_list, int order){
	uint64_t limit = 1<<order;
	uint64_t index;
	for(uint64_t i = 0; i <MAX_NUMBER_PAGES; i++){
		if(free_list[i].frame_addr == page){
			free_list[i].ref_count -= 1;
			if(free_list[i].ref_count == 0)
				free_list[i].is_free = 1;
			index = i;
			for(uint64_t i = index+1 ; i <index+limit; i++){
				free_list[i].ref_count -= 1;
				if(free_list[i].ref_count == 0)
					free_list[i].is_free = 1;
			}
			break;
		}
	}

}
uint64_t * get_free_frame() {
	uint64_t freePage = get_free_page(free_list);
	printf("returning freepage:%p ", freePage);
	return (uint64_t *) freePage;
}
uint64_t * get_free_frames(int order){
	uint64_t freePage = get_free_pages(free_list, order);
	printf("returning freepages:%p ", freePage);
	return (uint64_t *) freePage;
}