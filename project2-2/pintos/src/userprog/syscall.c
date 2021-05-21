#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
static void syscall_handler(struct intr_frame *);
void check_validity(void *);
struct file* find_by_fd(int fd);
//static bool writing = false;
struct file *find_by_fd(int fd)
{
	struct list *the_list = &thread_current()->list_of_file_struct;
	struct list_elem *e;
	for (e = list_begin(the_list); e != list_end(the_list); e = list_next(e))
	{
		struct file_structure *file_str = list_entry(e, struct file_structure, file_list_elem);
		if (file_str->fd == fd)
		{
			return file_str->the_file;
		}
	}
	return NULL;
}

void syscall_init(void)
{
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_validity(void *the_pointer)
{
	if (the_pointer == NULL ||
			!is_user_vaddr(the_pointer) ||
			pagedir_get_page(thread_current()->pagedir, the_pointer) == NULL)
	{
		thread_current()->exit_status = -1;
		thread_exit();
	}
}
static void
syscall_handler(struct intr_frame *f)
{
	check_validity((void *)f->esp);
	switch (*(int *)f->esp)
	{
	case SYS_WRITE:
	{
		check_validity((void *)((int *)f->esp + 5));
		check_validity((void *)((int *)f->esp + 6));
		check_validity((void *)((int *)f->esp + 7));
		int fd = *((int *)f->esp + 5);
		void *buffer = (void *)(*((int *)f->esp + 6));
		check_validity((void *)*((int *)f->esp + 6));
		unsigned size = *((unsigned *)f->esp + 7);
		if (fd == 1)
		{
			putbuf(buffer, size);
			f->eax = size;
			break;
		}
		else
		{
			struct file *the_file= find_by_fd(fd);
			if(the_file!= NULL) file_allow_write(the_file);
			if (the_file != NULL)
			{
//				if(writing){
//					f->eax = 0;
//					break;
//				}else{
//					writing = true;
					f->eax = file_write(the_file, buffer, size);
//					writing = false;					
					file_deny_write(the_file);
					break;
//					}
			}
			else
			{
				f->eax = 0;
				break;
			}
		}
		break;
	}
	case SYS_EXIT:
	{
		//int* temp = f->esp;
		//temp++;
		check_validity((void *)((int *)f->esp + 1));
		int status = *((int *)f->esp + 1);
		f->eax = status;
		thread_current()->exit_status = status;
		thread_exit();
		break;
	}
	case SYS_CREATE:
	{
		check_validity((void *)((int *)f->esp + 4));
		check_validity((void *)((int *)f->esp + 5));
		check_validity((void *)*((int *)f->esp + 4));
		const char *file = (const char *)*((int *)f->esp + 4);
		unsigned initial_size = (unsigned)*((int *)f->esp + 5);
		if (file == NULL)
		{
			f->eax = false;
			break;
		}
		else
		{
			
			bool temp = filesys_create(file, initial_size);
			
			f->eax = temp;
			break;
		}
	}
	case SYS_OPEN:
	{
		check_validity((void *)((int *)f->esp + 1));
		check_validity((void *)*((int *)f->esp + 1));
		const char *file = (const char *)*((int *)f->esp + 1);
		if (file == NULL)
		{
			f->eax = -1;
			break;
		}
		else
		{
			
			struct file *the_file = filesys_open(file);
			
			if (the_file != NULL)
			{	
				//file_deny_write(the_file);
				struct thread *cur = thread_current();
				struct list *the_list = &cur->list_of_file_struct;
				int fd = cur->max_fd;
				cur->max_fd += 1;
				create_file_struct(the_file, fd, the_list);
				f->eax = fd;
				file_allow_write(the_file);
				break;
			}
			else
			{

				f->eax = -1;
				break;
			}
		}
		break;
	}
	case SYS_FILESIZE:
	{
		check_validity((void *)((int *)f->esp + 1));
		int fd = *((int *)f->esp + 1);
		struct file *the_file = find_by_fd(fd);
		f->eax = file_length(the_file);
		break;
	}
	case SYS_READ:
	{
		check_validity((void *)((int *)f->esp + 5));
		check_validity((void *)((int *)f->esp + 6));
		check_validity((void *)((int *)f->esp + 7));
		int fd = *((int *)f->esp + 5);
		void *buffer = (void *)(*((int *)f->esp + 6));
		check_validity((void *)*((int *)f->esp + 6));
		unsigned size = *((unsigned *)f->esp + 7);
		struct file *the_file = find_by_fd(fd);
		if (fd == 0)
		{
			int i;
			int temp_size = 0;
			for (i = 0; i < size; i++)
			{
				uint8_t the_char = input_getc();
				buffer = the_char;
				buffer++;
				temp_size++;
			}
			f->eax = temp_size;
			break;
		}
		else
		{
			if (the_file != NULL)
			{
				file_deny_write(the_file);
		//		writing = true;
				f->eax = file_read(the_file, buffer, size);
		//		writing = false;
				file_allow_write(the_file);
				break;
			}
			else
			{
				f->eax = -1;
				break;
			}
		}
		break;
	}
	case SYS_EXEC:
	{
		check_validity((void*)((int*)f->esp +1));
		check_validity((void*)*((int*)f->esp+1)); 
		const char* cmd_line = (const char*)*((int*)f->esp + 1);
		// printf("exec_begin: \n%s \n", thread_current()->name);
		int tid = process_execute(cmd_line);
		if(tid == TID_ERROR){
			f->eax = -1;
			break;
		}
		struct thread* exec_child = findby_thread_tid(tid);
		exec_child->exec_parent_tid = thread_current()->tid;
		exec_child->has_exec_parent = true;
		sema_init(&thread_current()->exec_sema, 0);
		sema_down(&thread_current()->exec_sema);
		// printf("exec_after: \n%s \n", thread_current()->name);
		if(thread_current()->exec_child_exit_status == -1){
			f->eax = -1;
			break;
		}else{
			f->eax = tid;
			break;
		}
	}
	case SYS_WAIT:
	{
		check_validity((void*)((int*)f->esp +1));
		int pid = *((int*)f->esp +1);
		if(pid == -1){
			f->eax = -1;
			break;
		}else{
			struct list_elem* e;
			struct list* l = &thread_current()->list_of_finished_childs;
			bool done = false;
			for(e = list_begin(l); e!=list_end(l); e = list_next(e)){
				
				struct finished_child* fc = list_entry(e, struct finished_child, child_elem);
		
				if(fc->tid == pid){
				
					if(fc->waited==true){
						done = true;
						f->eax = -1;
						break;
					}else{
						done = true;
						fc->waited = true;
						f->eax = fc->exit_status;
						break;
					}
				}
			}
			if(done) break;		
			else{
				f->eax = process_wait(pid);
				break;
			}
		}
	}
	case SYS_CLOSE:
	{
		check_validity((void *)((int *)f->esp + 1));
		int the_fd = *((int *)f->esp + 1);
		struct thread *cur = thread_current();
		struct list *the_list = &cur->list_of_file_struct;
		struct list_elem *e;
		bool found_closed = false;
		for (e = list_begin(the_list); e != list_end(the_list); e = list_next(e))
		{
			struct file_structure *f = list_entry(e, struct file_structure, file_list_elem);
			if (f->fd == the_fd)
			{
				//found_closed = true;
				file_close(f->the_file);
//				file_deny_write(f->the_file);
				list_remove(&f->file_list_elem);
				free(f);
				break;
			}
		}
		//ASSERT(found_closed);
		break;
	}
	case SYS_REMOVE:
	{
		check_validity((void *)((int *)f->esp + 1));
		check_validity((void *)*((int *)f->esp + 1));
		const char *file = (const char *)*((int *)f->esp + 1);
		f->eax = filesys_remove(file);
		break;
	}
	case SYS_TELL:
	{
		check_validity((void *)((int *)f->esp + 1));
		int fd = *((int *)f->esp + 1);
		struct file *the_file = find_by_fd(fd);
//		file_deny_write(the_file);
		f->eax = 1 + file_tell(the_file);
		break;
	}
	case SYS_SEEK:
	{
		check_validity((void *)((int *)f->esp + 4));
		check_validity((void *)((int *)f->esp + 5));
		unsigned pos = (unsigned *)*((int *)f->esp + 5);
		int fd = *((int *)f->esp + 4);
		struct file *the_file = find_by_fd(fd);
//		file_deny_write(the_file);
		f->eax = 1 + file_seek(the_file, pos);
		break;
	}
	case SYS_HALT:
	{
		shutdown_power_off();
		break;
	}
	}
}
