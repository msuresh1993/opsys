#include<sys/paging.h>
#include <sys/kmalloc.h>
#include <sys/sbunix.h>
#include <sys/process.h>
#include <sys/scheduling.h>
#include <sys/freelist.h>
#include <sys/isr_stuff.h>
#include <sys/defs.h>
#include <sys/utils.h>
extern task_struct_t *currenttask;

void cp_prev_next(task_struct_t* from, task_struct_t* to) {
	add_process_runq(to);
}

void cp_executable(task_struct_t * from, task_struct_t * to) {
	char *q = to->executable;
	char *p = from->executable;
	while (*p) {
		*q = *p;
		p++;
		q++;
	}
}

uint64_t find_stack_page_order(char* stk_strt, char* stk_to) {
	uint64_t order = (uint64_t) (align((uint64_t )(stk_strt), PAGE_ALIGN))
			- (uint64_t) (align(((uint64_t )(stk_to) - 0x1000), PAGE_ALIGN));
	order = order >> 12;
	order = next_power_two(order);
	order = find_order(order);
	return order;
}

void cp_process_stack(task_struct_t * from, task_struct_t * to,
		pv_map_t* usr_stk_pv_map) {

	char * stk_strt = (char *) (0x7000000 + 0x500); //todo: hard coding
	char * stk_to = (char *) from->state.rsp;

	uint64_t order = find_stack_page_order(stk_strt, stk_to);
	uint64_t phys = (uint64_t) get_free_frames(order);
	uint64_t virtual_addr = (uint64_t) get_virtual_location(order);

	uint64_t stk_virt_base = from->state.rsp & ~(0xfff);
	for (uint64_t i = 0; i < (1 << order); i++) {
		uint64_t virt = (virtual_addr + i * 0x1000); // virt addr only to access phys page now
		uint64_t physi = (phys + i * 0x1000);
		setup_kernel_page_tables(virt, physi);
		uint64_t usr_stk_virt = (stk_virt_base + i * 0x1000); // target user stack virtual addr
		cache_pv_mapping(usr_stk_pv_map, usr_stk_virt, physi);
	}
	char * source = stk_to; // stack grows downwards
	int offset = from->state.rsp & 0xfff; // last 3 nibbles
	char * target = ((char *) virtual_addr) + offset;

	while (source <= stk_strt) {
		*target = *source;
		target++;
		source++;
	}

	to->user_stk_kmalloc_addr = 0; // we dont need this field for user processes
}

void cp_kernel_stack(task_struct_t * from, task_struct_t * to,
		pv_map_t * krnl_stk_pv, uint64_t stack_top) {
//	uint64_t virtual_addr = *stack_virt;

	uint64_t stack_kernel = set_get_kernel_stck_addr(to);
	to->state.kernel_rsp = (stack_kernel + 0xfff);


	// 2 pages of kernel stack will be mapped in page table
	cache_pv_mapping(krnl_stk_pv, stack_kernel,
			phys_addr_of_frame(stack_kernel));
	cache_pv_mapping(krnl_stk_pv, to->state.kernel_rsp,
			phys_addr_of_frame(to->state.kernel_rsp));

	uint64_t *temp = (uint64_t *) (to->state.kernel_rsp);
	regs_syscall_t *from_virtual = (regs_syscall_t *) (stack_top);
	*temp = from_virtual->ss;
	temp -= 1;
	*temp = from_virtual->rsp;
	temp -= 1;
	*temp = from_virtual->flag;
	temp -= 1;
	*temp = from_virtual->cs;
	temp -= 1;
	*temp = from_virtual->rip;
	temp -= 1;
	*temp = 0;
	temp -= 1;
	*temp = from_virtual->rbx;
	temp -= 1;
	*temp = from_virtual->rcx;
	temp -= 1;
	*temp = from_virtual->rdx;
	temp -= 1;
	*temp = from_virtual->rdi;
	temp -= 1;
	*temp = from_virtual->rsi;
	temp -= 1;
	*temp = from_virtual->rbp;
	temp -= 1;
	*temp = from_virtual->r8;
	temp -= 1;
	*temp = from_virtual->r9;
	temp -= 1;
	*temp = from_virtual->r10;
	temp -= 1;
	*temp = from_virtual->r11;
	temp -= 1;
	*temp = from_virtual->r12;
	temp -= 1;
	*temp = from_virtual->r13;
	temp -= 1;
	*temp = from_virtual->r14;
	temp -= 1;
	*temp = from_virtual->r15;
	temp -= 1;
	*temp = from_virtual->ds;
	temp -= 1;
	*temp = from_virtual->es;
	temp -= 1;
	*temp = from_virtual->fs;
	temp -= 1;
	*temp = from_virtual->gs;
	to->state.kernel_rsp = (uint64_t) temp;
}

void cp_pstate(task_struct_t * from, task_struct_t * to) {
	process_state fstate = from->state;

	to->state.rsp = fstate.rsp;
	to->state.rip = fstate.rip;
	//general registers

	//not copying cr3

	//flags
	to->state.flags = fstate.flags;
	//segment registers

	to->p_state = from->p_state;
	to->waiting_for = DEFAULT_WAITING_FOR;
	to->is_kernel_process = from->is_kernel_process;

	copy_file_dp_process(to, from);
}

void cp_vma(vma_t* from, vma_t* to, mem_desc_t *mem_map) {
	to->my_mem_desc = mem_map;
	to->vma_start = from->vma_start;
	to->vma_end = from->vma_end;
	to->vm_perm = from->vm_perm;
	to->type = from->type;
}

vma_t* cp_linked_vmas(vma_t* from_vma, mem_desc_t* to) {
	vma_t* prev = NULL;
	vma_t* head = NULL;
	while (from_vma != NULL) {
		vma_t* vma = kmalloc(sizeof(vma_t));
		vma->vma_next = NULL;
		cp_vma(from_vma, vma, to);

		if (prev != NULL) {
			// not first
			prev->vma_next = vma;
		} else {
			// first
			head = vma;
		}

		from_vma = from_vma->vma_next;
		prev = vma;
	}
	return head;
}

void cp_vma_list(mem_desc_t * from, mem_desc_t * to) {
	vma_t *from_vma = from->vma_list;
	vma_t* vma_head = cp_linked_vmas(from_vma, to);
	to->vma_list = vma_head;
}

void cp_vma_cache(mem_desc_t * from, mem_desc_t * to) {
	vma_t *from_vma = from->vma_cache;
	vma_t* vma_head = cp_linked_vmas(from_vma, to);
	to->vma_cache = vma_head;
}

void cp_mem_desc(task_struct_t * from, task_struct_t * to) {
	mem_desc_t *tmem_map = kmalloc(sizeof(mem_desc_t));
	to->mem_map = tmem_map;
	mem_desc_t *fmem_map = from->mem_map;

	cp_vma_list(fmem_map, tmem_map);
	cp_vma_cache(fmem_map, tmem_map);

	tmem_map->mm_users = fmem_map->mm_users;
	tmem_map->mm_count = fmem_map->mm_count;
	tmem_map->num_vma = fmem_map->num_vma;

	//todo: below
//	tmem_map->mem_desc_head = fmem_map->

	tmem_map->text_elf_addr = fmem_map->text_elf_addr;
	tmem_map->rodata_elf_addr = fmem_map->rodata_elf_addr;
	tmem_map->data_elf_addr = fmem_map->data_elf_addr;
	tmem_map->ehframe_elf_addr = fmem_map->ehframe_elf_addr;
	tmem_map->got_elf_addr = fmem_map->got_elf_addr;
	tmem_map->gotplt_elf_addr = fmem_map->gotplt_elf_addr;

	tmem_map->start_code = fmem_map->start_code;
	tmem_map->end_code = fmem_map->end_code;
	tmem_map->start_data = fmem_map->start_data;
	tmem_map->end_data = fmem_map->end_data;
	tmem_map->start_brk = fmem_map->start_brk;
	tmem_map->brk = fmem_map->brk;
	tmem_map->start_stack = fmem_map->start_stack;
	tmem_map->arg_start = fmem_map->arg_start;
	tmem_map->arg_end = fmem_map->arg_end;
	tmem_map->env_start = fmem_map->env_start;
	tmem_map->env_end = fmem_map->env_end;
}

// pte which contains the page that contains
// virtual_addr is marked as read only
void set_pte_as_read(uint64_t virtual_addr) {
	if (is_linear_addr_mapped(virtual_addr)) {
		uint64_t * pte = virtual_addr_pte(virtual_addr);
		*pte = set_bit(*pte, 1, 0); // bit 1 as 0
		invalidate_addresses_with_page((uint64_t *) virtual_addr);
	}
}

int is_stack_vma(vma_t* vma) {
	return vma->type == 4;
}

// mark pages of parent process as read only
// DO NOT DO THIS FOR STACK
void mark_pages_read(task_struct_t * from) {
	mem_desc_t *fmem_map = from->mem_map;
	vma_t *vma = fmem_map->vma_list;
	while (vma != NULL) {
		for (uint64_t addr = vma->vma_start; addr < vma->vma_end; addr +=
				4096) {
			if (!is_stack_vma(vma)) // don't mess up with stack
				set_pte_as_read(addr);
		}
		vma = vma->vma_next;
	}
}

//// lookup virtual addr for physical addr
//int phys_to_virt(uint64_t phys_addr, pv_map_t * pv_node, uint64_t *vaddr) {
//	if (pv_node == NULL) {
//		return 0;
//	} else {
//		pv_map_t *temp = pv_node;
//		while (temp != NULL) {
//			if (temp->physical_addr == phys_addr) {
//				*vaddr = temp->virtual_addr;
//				return 1;
//			}
//			temp = temp->next;
//		}
//		return 0;
//	}
//
//}
//
//void insert_into_map(pv_map_t* pv_map_node, uint64_t phys, uint64_t virt) {
//	if(*pv_map_node == NULL){
//		*pv_map_node = kmalloc()
//	}
//}

// page tables that are involved in resolving page_base are
// created if not already created and entries are duplicated
void cp_ptables_for(uint64_t page_base, pv_map_t* pv_map_node,
		uint64_t ** chld_pml4_base_dbl_ptr) {
	// parent process has not touched this addr, so why should we bother
	// creating page table entry for this in the child? Yes, we don't.
	if (!is_linear_addr_mapped(page_base)) {
//		printf(" not mapped in parent, ignoring copying %p ", page_base);
		return;
	}
	// store this pte val for the addr page_base in the child page table too
	uint64_t * target_pte = virtual_addr_pte(page_base);
	uint64_t page_phys_addr = (uint64_t) next_entity_base(target_pte);

	// next_entity_virtual_base is a function similar to
	// next_entity_base in paging.c, except this one returns
	// a virtual address of the next entity base, as once cr3
	// is updated, we need virtual addr to access any location
	// remaining functionality is identical to that of setting
	// page table before cr3 update - climb down others' page tables
	// and update page table entries.

	// since page tables need to be accessible for the kernel setting
	// present, write, supervisor
	// ---- TODO - change to read-only apt permission below ----
	if (if_not_contains_virt_addr(pv_map_node, page_base, page_phys_addr)) {
		setup_process_page_table_from_outside(page_base, page_phys_addr, 1, 0,
				1, chld_pml4_base_dbl_ptr, phys_to_virt_map, pv_map_node);
//		printf(" in forkk count before for %p is %d ", page_phys_addr,
//				get_ref_count(page_phys_addr));
		increase_ref_count(page_phys_addr);
		cache_pv_mapping(pv_map_node, page_base, page_phys_addr);
	}

}

pv_map_t* cp_for_each_vma(vma_t* vma, uint64_t** child_pml_dbl_ptr_virtual,
		pv_map_t* pv_map_node) {
	while (vma != NULL) {
		if (!is_stack_vma(vma)) {
			for (uint64_t addr = vma->vma_start; addr < vma->vma_end; addr +=
					4096) {
				uint64_t page_base = addr & (~0xfff);
//				printf("vma_begin:%p vma_end:%p, type:%d copying %p ",
//						vma->vma_start, vma->vma_end, vma->type, page_base);
				cp_ptables_for(page_base, pv_map_node,
						child_pml_dbl_ptr_virtual);
			}
		}
		vma = vma->vma_next;
	}

	return pv_map_node;
}

// except stack page tables
void cp_page_tables(task_struct_t * from, task_struct_t * to,
		pv_map_t * krnl_stk_pv, pv_map_t * usr_stk_pv) {
	mem_desc_t *fmem_map = from->mem_map;
	vma_t* vma = fmem_map->vma_list;

	uint64_t child_pml_virtual = 0;
	uint64_t child_pml_phys = 1;
	get_both_pml4_base_addrs_for_process(&child_pml_virtual, &child_pml_phys);
	to->state.cr3 = child_pml_phys;

	uint64_t * child_pml_virtual_ptr = (uint64_t *) child_pml_virtual;
	uint64_t** child_pml_dbl_ptr_virtual = &child_pml_virtual_ptr;

	pv_map_t* pv_map_node = init_pv_map();
	pv_map_node = cp_for_each_vma(vma, child_pml_dbl_ptr_virtual, pv_map_node);

	for (pv_map_t* p = krnl_stk_pv; p != NULL; p = p->next) {
		// map the kernel stack, which could span across multiple pages
		setup_kernel_page_table_from_outside(p->virtual_addr, p->physical_addr,
				1, 1, 1, child_pml_dbl_ptr_virtual, phys_to_virt_map,
				pv_map_node);
	}

	for (pv_map_t* p = usr_stk_pv; p != NULL; p = p->next) {
		// map the process stack, which could span across multiple pages
		setup_process_page_table_from_outside(p->virtual_addr, p->physical_addr,
				1, 1, 1, child_pml_dbl_ptr_virtual, phys_to_virt_map,
				pv_map_node);
	}

//	free_pv_map(pv_map_node);
}

void copy_tsk(uint64_t pid, task_struct_t * from, task_struct_t * to,
		uint64_t stack_top) {
	to->pid = pid;
	to->ppid = from->pid;
	to->is_background = from->is_background;
	strcpy(to->pwd, from->pwd);
	cp_executable(from, to);
	cp_pstate(from, to);
	cp_mem_desc(from, to);
	mark_pages_read(from);
	cp_prev_next(from, to);

	pv_map_t* krnl_stk_pv = init_pv_map();
	from->state.kernel_rsp = stack_top;
	cp_kernel_stack(from, to, krnl_stk_pv, stack_top);

	pv_map_t* usr_stk_pv = init_pv_map();
	cp_process_stack(from, to, usr_stk_pv);
	to->state.rsp = from->state.rsp;

	cp_page_tables(from, to, krnl_stk_pv, usr_stk_pv);
//	free_pv_map(usr_stk_pv);
//	free_pv_map(krnl_stk_pv);
}
void foob(){}
void copy_process(uint64_t pid, uint64_t stack_top) {
	task_struct_t *task = kmalloc(sizeof(task_struct_t));
	copy_tsk(pid, currenttask, task, stack_top);
	foob();
}

int do_fork(uint64_t stack_top) {
	uint64_t pid = get_next_pid();
//	printf("forked is %d ", pid);
	copy_process(pid, stack_top);
	return (int) pid;
}

