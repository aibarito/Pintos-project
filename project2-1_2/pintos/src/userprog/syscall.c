#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
static void syscall_handler (struct intr_frame *);
void check_validity(void*);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_validity(void* the_pointer){
	if(the_pointer == NULL || 
		!is_user_vaddr(the_pointer) || 
			pagedir_get_page(thread_current()->pagedir, the_pointer) == NULL )
	{
		thread_current() -> exit_status = -1;
		thread_exit();
	}
}
static void
syscall_handler (struct intr_frame *f) 
{
  check_validity((void*)f->esp);
  switch(*(int*)f->esp){
	case SYS_WRITE:
	{
		check_validity((void*)((int*)f->esp + 5));
		check_validity((void*)((int*)f->esp+6));
		check_validity((void*)((int*)f->esp +7));
		int fd = *((int*)f->esp + 5);		
		void* buffer = (void*)(*((int*)f->esp + 6));
		unsigned size = *((unsigned*)f->esp + 7);
		if(fd == 1){		
			putbuf(buffer, size);		
			f->eax = size;
		}
		else{}
		break;
	}
	case SYS_EXIT:
	{
		//int* temp = f->esp;
		//temp++;
		check_validity((void*)((int*)f->esp + 1));
		int status = *((int*)f->esp + 1);
		f->eax = status;
		thread_current() -> exit_status = status;
		thread_exit();
		break;
	}
	case SYS_CREATE:
	{
		check_validity((void*)((int*)f->esp +4));
		check_validity((void*)((int*)f->esp+5));
		check_validity((void*)*((int*)f->esp+4));
		const char* file = (const char*)*((int*)f->esp + 4);
		unsigned initial_size = (unsigned)*((int*)f->esp + 5);
		if(file == NULL){
			f->eax = false;
			break;
		}else{		
			bool temp = filesys_create(file, initial_size);
			f->eax = temp;
			break;
		}
	}
	case SYS_OPEN:
	{
		check_validity((void*)((int*)f->esp +1));
		check_validity((void*)*((int*)f->esp+1));
		const char* file = (const char*)*((int*)f->esp + 1);
		if(file == NULL) {
			f->eax = -1;
			break;
		}
		else{
			struct file* the_file = filesys_open(file);
			if(the_file !=NULL){
				struct thread* cur = thread_current();
				struct list* the_list = &cur->list_of_file_struct;
				int fd = cur->max_fd;
				cur -> max_fd += 1;
				create_file_struct(the_file, fd, the_list);
				f->eax = fd;
				break;
			}else{
				f->eax = -1;
				break;
			}
		}
	}
	case SYS_CLOSE:
	{
		check_validity((void*)((int*)f->esp + 1));
		int the_fd = *((int*)f->esp + 1);
		struct thread* cur = thread_current();
		struct list* the_list = &cur->list_of_file_struct;
		struct list_elem *e;
		bool found_closed = false;
		for(e = list_begin(the_list); e!=list_end(the_list); e= list_next(e)){
			struct file_structure* f = list_entry(e, struct file_structure, file_list_elem);
			if(f->fd == the_fd){
				//found_closed = true;
				file_close(f->the_file);
				list_remove(&f->file_list_elem);
				free(f);
				break;
			}
		}	
		//ASSERT(found_closed);
		break;
	}
  }
 
}

