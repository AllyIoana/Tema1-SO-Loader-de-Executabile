/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "exec_parser.h"

static so_exec_t *exec;
static int exec_file;
static struct sigaction default_handler;


static void segv_handler(int signum, siginfo_t *info, void *context)
{
	int i, found_seg = -1, current_page, pages_no, page_size, page_start;
	uintptr_t current_address;
	void *map;

	if (signum != SIGSEGV) {
		default_handler.sa_sigaction(signum, info, context);
		return;
	}

	// finding the segment's index in the executable's segment array
	for (i = 0; i < exec->segments_no; i++)
		if ((uintptr_t)(info->si_addr) <= exec->segments[i].vaddr + exec->segments[i].mem_size)
			if ((uintptr_t)(info->si_addr) >= exec->segments[i].vaddr) {
				found_seg = i;
				break;
			}

	// segment not found -> using the default handler
	if (found_seg == -1) {
		default_handler.sa_sigaction(signum, info, context);
		return;
	}

	// calculating the number of pages
	page_size = getpagesize();
	pages_no = exec->segments[found_seg].mem_size / page_size + 1;

	// // finding the current page
	current_page = ((uintptr_t)info->si_addr - exec->segments[found_seg].vaddr) / page_size;
	current_address = exec->segments[found_seg].vaddr + current_page * page_size;

	// checking if the current page is already mapped
	char *indexes; // contains the indexes of the mapped pages: the last one is -1
	if (exec->segments[found_seg].data == NULL) {
		indexes = malloc((pages_no + 1) * sizeof(char));
		indexes[0] = current_page;
		indexes[1] = -1;
		exec->segments[found_seg].data = indexes;
	} else {
		indexes = exec->segments[found_seg].data;
		for (i = 0; i < pages_no && indexes[i] != -1; i++) {
			// current page is already mapped -> default handler
			if (indexes[i] == current_page) {
				default_handler.sa_sigaction(signum, info, context);
				return;
			}
		}
		if (indexes[i] == -1) {
			indexes[i] = current_page;
			indexes[i + 1] = -1;
		}
	}

	// maping the current page
	map = mmap((void *)current_address, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
	// the start of the current page in the file
	page_start = exec->segments[found_seg].offset + current_page * page_size;
	lseek(exec_file, page_start, SEEK_SET);

	// reading from file and modifying the permissions
	char *buffer = malloc(sizeof(char) * page_size);

	if (exec->segments[found_seg].file_size <= current_page * page_size)
		return;
	else if ((exec->segments[found_seg].file_size - current_page * page_size) >= page_size) {
		if (read(exec_file, buffer, page_size) == -1)
			printf("Error with read function\n");
		if (memcpy(map, buffer, page_size) == NULL)
			printf("Error with memcpy function\n");
		free(buffer);
		if (mprotect(map, page_size, exec->segments[found_seg].perm) == -1)
			printf("Error with mprotect function\n");
	} else {
		if (read(exec_file, buffer, (exec->segments[found_seg].file_size - current_page * page_size)) == -1)
			printf("Error with read function\n");
		if (memcpy(map, buffer, (exec->segments[found_seg].file_size - current_page * page_size)) == NULL)
			printf("Error with memcpy function\n");
		free(buffer);
		if (mprotect(map, page_size, exec->segments[found_seg].perm) == NULL)
			printf("Error with mprotect function\n");
	}
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGSEGV);
	rc = sigaction(SIGSEGV, &sa, &default_handler);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	close(exec_file);
	return 0;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	if (!exec)
		return -1;
	exec_file = open(path, O_RDONLY);
	so_start_exec(exec, argv);

	return -1;
}
