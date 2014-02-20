/*
 * Copyright (c) 2013, Lawrence Livermore National Security, LLC.  
 * Produced at the Lawrence Livermore National Laboratory  
 * Written by Barry Rountree, rountree@llnl.gov.
 * All rights reserved. 
 * 
 * This file is part of libmsr.
 * 
 * libmsr is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 * 
 * libmsr is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License along
 * with libmsr.  If not, see <http://www.gnu.org/licenses/>. 
 */

// Necessary for pread & pwrite.
#define _XOPEN_SOURCE 500

#include <stdio.h>	//   perror
#include <unistd.h>	//   pread, pwrite
//#include <sys/types.h>  // \ ....
#include <sys/stat.h> 	// | open, fstat
#include <fcntl.h>	// / ....
#include <stdint.h>	// uint64_t
#include <errno.h>
#include "msr_core.h"

int msr_debug;
static int core_fd[NUM_SOCKETS][NUM_CORES_PER_SOCKET];

int
init_msr(){
	int i,j;
	char filename[1025];
	struct stat statbuf;
	static int initialized = 0;
	int retVal;

	if( initialized ){
		return 0;
	}
	for (i=0; i<NUM_SOCKETS; i++){
		for (j=0; j<NUM_CORES_PER_SOCKET; j++){
			// Open the rest of the cores for core-level msrs.  
			snprintf(filename, 1024, "/dev/cpu/%d/msr_safe", i*NUM_CORES_PER_SOCKET+j);

			retVal = stat(filename, &statbuf);

			if (retVal == -1)
			  return msr_msg(MSR_ERROR, 
					 "Error: stat failed on %s; check if msr module is loaded", 
					 filename);
		
			if(!(statbuf.st_mode & S_IRUSR) || !(statbuf.st_mode & S_IWUSR))
			  return msr_msg(MSR_ERROR, 
					 "Read/write permissions denied on %s", filename);

			core_fd[i][j] = open( filename, O_RDWR );

			if(core_fd[i][j] == -1)
			  return msr_msg(MSR_ERROR, 
					 "Error opening %s; check if msr module is loaded.", 
					 filename);
		}
	}

	initialized = 1;

	return 0;
}

void 
finalize_msr(){
	int i, j, rc;
	char filename[1025];
	for( i=0; i<NUM_SOCKETS; i++){
		for(j=0; j<NUM_CORES_PER_SOCKET; j++){
			if(core_fd[i][j]){
				rc = close(core_fd[i][j]);
				if( rc != 0 ){
					snprintf(filename, 1024, "%s::%d  Error closing file /dev/cpu/%d/msr\n", 
							__FILE__, __LINE__, i*NUM_CORES_PER_SOCKET+j);
					perror(filename);
				}
			}else{
				core_fd[i][j] = 0;
			}
		}
	}
}

void
write_msr(int socket, off_t msr, uint64_t val){
	write_msr_single_core( socket, 0, msr, val );
}

void
write_msr_all_cores(int socket, off_t msr, uint64_t val){
	int j;
	for(j=0; j<NUM_CORES_PER_SOCKET; j++){
		write_msr_single_core( socket, j, msr, val );
	}
}

void
write_msr_all_cores_v(int socket, off_t msr, uint64_t *val){
	int j;
	for(j=0; j<NUM_CORES_PER_SOCKET; j++){
		write_msr_single_core( socket, j, msr, val[j] );
	}
}

void
write_msr_single_core(int socket, int core, off_t msr, uint64_t val){
	int rc, core_fd_idx;
	char error_msg[1025];
	uint64_t actual;
	core_fd_idx = socket*NUM_CORES_PER_SOCKET+core;
	rc = pwrite( core_fd[socket][core], &val, (size_t)sizeof(uint64_t), msr );
	if( rc != sizeof(uint64_t) ){
		snprintf( error_msg, 1024, "%s::%d  pwrite returned %d.  core_fd[%d][%d]=%d, socket=%d, core=%d socket+core=%d msr=%ld (0x%lx).  errno=%d\n", 
				__FILE__, __LINE__, rc, socket, core, core_fd[socket][core], socket, core, core_fd_idx, msr, msr, errno );
		perror(error_msg);
	}

	//Verify the value that was written
	 rc = pread(core_fd[socket][core], &actual, (size_t)sizeof(uint64_t), msr);
	if( rc != sizeof(uint64_t) ){
                snprintf( error_msg, 1024, "%s::%d  Verifying the value that was written: pread returned %d.  core_fd[%d][%d]=%d, socket=%d, core=%d socket+core=%d msr=%ld (0x%lx).  errno=%d\n",
                                __FILE__, __LINE__, rc, socket, core, core_fd[socket][core], socket, core, core_fd_idx, msr, msr, errno );
                perror(error_msg);
        }
	if(actual == val){
		fprintf(stderr,"writemsr: Verification successful. core_fd[%d][%d]=%d, socket=%d, core=%d socket+core=%d msr=%ld (0x%lx).\n", socket, core, core_fd[socket][core], socket, core, core_fd_idx, msr,msr);
	}
	else {
             snprintf(error_msg, 1024, "%s::%d  writemsr: verification failed. core_fd[%d][%d]=%d, socket=%d, core=%d socket+core=%d msr=%ld (0x%lx).  errno=%d\n",
                                __FILE__, __LINE__, socket, core, core_fd[socket][core], socket, core, core_fd_idx, msr, msr, errno );
	}

}

void
read_msr(int socket, off_t msr, uint64_t *val){
	read_msr_single_core(socket, 0, msr, val);
}

void
read_msr_all_cores_v(int socket, off_t msr, uint64_t *val){
	int j;
	for(j=0; j<NUM_CORES_PER_SOCKET; j++){
		read_msr_single_core(socket, 0, msr, &val[j]);
	}
}

void 
read_msr_single_core(int socket, int core, off_t msr, uint64_t *val){
	int rc, core_fd_idx;
	char error_msg[1025];
	core_fd_idx = socket*NUM_CORES_PER_SOCKET+core;
	rc = pread( core_fd[socket][core], (void*)val, (size_t)sizeof(uint64_t), msr );
	if( rc != sizeof(uint64_t) ){
		snprintf( error_msg, 1024, "%s::%d  pread returned %d.  core_fd[%d][%d]=%d, socket=%d, core=%d socket+core=%d msr=%ld (0x%lx).  errno=%d\n", 
				__FILE__, __LINE__, rc, socket, core, core_fd[socket][core], socket, core, core_fd_idx, msr, msr, errno );
		perror(error_msg);
	}
}

