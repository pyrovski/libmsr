/* msr_clocks.c
 *
 * MPERF, APERF and friends.
 */

#include <stdio.h>
#include "msr_core.h"
#include "msr_clocks.h"
#include <assert.h>


#define MSR_IA32_MPERF 		0x000000e7
#define MSR_IA32_APERF 		0x000000e8
#define IA32_TIME_STAMP_COUNTER 0x00000010
#define IA32_CLOCK_MODULATION		(0x19A) // If Hyper-Threading Technology enabled processors, 
						// The IA32_CLOCK_MODULATION register is duplicated
						// for each logical processor. 
						// Must have it enabled and the same for all logical
						// processors within the physical processor

void 
read_all_aperf(uint64_t *aperf){
	read_all_threads( MSR_IA32_APERF, aperf );
}

void
read_all_mperf(uint64_t *mperf){
	read_all_threads( MSR_IA32_MPERF, mperf );
}

void 
read_all_tsc(uint64_t *tsc){
	read_all_threads( IA32_TIME_STAMP_COUNTER, tsc );
}

void
dump_clocks_terse_label(){
	int thread_idx;
	for(thread_idx=0; thread_idx<NUM_THREADS; thread_idx++){
		fprintf(stdout, "aperf%02d\tmperf%02d\ttsc%02d\t", 
			thread_idx, thread_idx, thread_idx);
	}
}

void
dump_clocks_terse(){
	uint64_t aperf_val[NUM_THREADS], mperf_val[NUM_THREADS], tsc_val[NUM_THREADS];
	int thread_idx;
	read_all_aperf(aperf_val);
	read_all_mperf(mperf_val);
	read_all_tsc  (tsc_val);
	for(thread_idx=0; thread_idx<NUM_THREADS; thread_idx++){
		fprintf(stdout, "0x%llx\t0x%llx\t0x%llx\t", 
			aperf_val[thread_idx], mperf_val[thread_idx], tsc_val[thread_idx]);
	}
}

//----------------------------Software Controlled Clock Modulation-----------------------------------------------
/*struct clock_mod{
	uint64_t raw;

// There is a bit at 0 that can be used for Extended On-Demand Clock Modulation Duty Cycle
// It is added with the bits 3:1. When used, the granularity of clock modulation duty cycle
// is increased to 6.25% as opposed to 12.5%
// To enable this, must have CPUID.06H:EAX[Bit 5] = 1
// I am not sure how to check that because otherwise bit 0 is reserved

	int duty_cycle;		// 3 binary digits
				// 0-7 in decimal
				//
				// Value	Duty Cycle
				//   0		 Reserved
				//   1		 12.5% (default)
				//   2		 25.0%
				//   3		 37.5%
				//   4		 50.0%
				//   5		 63.5%
				//   6		 75.0%
				//   7		 87.5%

	int duty_cycle_enable;	// Read/Write
};
*/
void dump_clock_mod(struct clock_mod *s)
{
	double percent = 0.0;
	if(s->duty_cycle == 0)
	{
		percent = 6.25;
	}
	else if (s->duty_cycle == 1)
	{
		percent = 12.5;
	}
	else if (s->duty_cycle == 2)
	{
		percent = 25.0;
	}
	else if (s->duty_cycle == 3)
	{
		percent = 37.5;
	}
	else if (s->duty_cycle == 4)
	{
		percent = 50.0;
	}
	else if (s->duty_cycle == 5)
	{
		percent = 63.5;
	}
	else if (s->duty_cycle == 6)
	{
		percent = 75.0;
	}
	else if (s->duty_cycle == 7)
	{
		percent = 87.5;
	}
	fprintf(stdout, "duty_cycle 		= %d	\npercentage\t\t= %.2f\n", s->duty_cycle, percent);
	fprintf(stdout, "duty_cycle_enable	= %d\n", s->duty_cycle_enable);
	fprintf(stdout, "\n");
}

void get_clock_mod(int socket, int core, struct clock_mod *s)
{
	read_msr_by_coord(socket, core, 0, IA32_CLOCK_MODULATION, &(s->raw));
	//s->raw = 64; // temp value
	s->duty_cycle = MASK_VAL(s->raw, 3, 1);			// specific encoded values for target duty cycle

	s->duty_cycle_enable = MASK_VAL(s->raw, 4, 4);		// On-Demand Clock Modulation Enable
								// 1 = enabled, 0 disabled
}

void set_clock_mod(int socket, int core, struct clock_mod *s)
{
	uint64_t msrVal;
	read_msr_by_coord(socket, core, 0, IA32_CLOCK_MODULATION, &msrVal);
	//msrVal = 64; // temp value
	assert(s->duty_cycle > 0 && s->duty_cycle <8);
	assert(s->duty_cycle_enable == 0 || s->duty_cycle_enable == 1);

	msrVal = (msrVal & (~(3<<1))) | (s->duty_cycle << 1);
	msrVal = (msrVal & (~(1<<4))) | (s->duty_cycle_enable << 4);

	write_msr_by_coord(socket, core, 0, IA32_CLOCK_MODULATION, msrVal);
}

//---------------------------------END CLOCK MODULATION FUNCTIONS-----------------------------------------------------------

