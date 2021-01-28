/*
 * Copyright (c) 2020 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * OS-Lab-2020 (i.e., ChCore) is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *   http://license.coscl.org.cn/MulanPSL
 *   THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 *   PURPOSE.
 *   See the Mulan PSL v1 for more details.
 */

// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <common/printk.h>
#include <common/types.h>

static inline __attribute__ ((always_inline))
u64 read_fp()
{
	u64 fp;
	__asm __volatile("mov %0, x29":"=r"(fp));
	return fp;
}

__attribute__ ((optimize("O1")))
int stack_backtrace()
{
	printk("Stack backtrace:\n");
	u64 fp = read_fp();
	u64 deeper_fp = fp;
	fp = *(u64 *)fp;
	u64 lr = 0;
	u64 arg0 = 0;
	u64 arg1 = 0;
	u64 arg2 = 0;
	u64 arg3 = 0;
	u64 arg4 = 0;
	do{
		lr = *(u64 *)(fp+8);
		arg0 = *(u64 *)(deeper_fp+16);
		arg1 = *(u64 *)(deeper_fp+8*3);
		arg2 = *(u64 *)(deeper_fp+8*4);
		arg3 = *(u64 *)(deeper_fp+8*5);
		arg4 = *(u64 *)(deeper_fp+8*6);
		printk("LR %lx FP %lx Args %lx %lx %lx %lx %lx\n", lr, fp, 
				arg0, arg1, arg2, arg3, arg4);
		deeper_fp = fp;
		fp = *(u64 *)fp;
		if(arg0 == 5){
			printk("LR %lx FP %lx Args %lx \n", *(u64 *)(fp+8), fp, *(u64 *)(fp-16));
			break;
		}
	}while(1);	

	return 0;
}
