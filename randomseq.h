/******************************************************************************/
/**
@file		randomseq.h
@author		Ramon Lawrence
@brief		Generates a sequence of random numbers between 0 and N.
            Source: https://preshing.com/20121224/how-to-generate-a-sequence-of-unique-random-integers/
@copyright	Copyright 2021
			The University of British Columbia,
            Ramon Lawrence		
@par Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

@par 1.Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.

@par 2.Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

@par 3.Neither the name of the copyright holder nor the names of its contributors
	may be used to endorse or promote products derived from this software without
	specific prior written permission.

@par THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
/******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

#ifndef RANDOMSEQ_H
#define RANDOMSEQ_H

typedef struct {
    uint32_t index;
    uint32_t seed1;
    uint32_t seed2;
    uint32_t size;
    uint32_t prime;
} randomseqState;


void randomseqInit(randomseqState *state)
{
    state->index = 0;    
    if (state->prime == 0)
    {   /* Find a prime nunmber. */
        if (state->size <= 100)
            state->prime = 103;
        else  if (state->size <= 1000)
             state->prime = 1019;
        else  if (state->size <= 10000)
             state->prime = 10007;
        else  if (state->size <= 100000)
             state->prime = 100003;
        else
             state->prime = 1000003;
    }
    state->seed1 = rand() % state->prime;
    state->seed2 = rand() % state->prime;       
}

uint32_t permuteQPR(randomseqState *state, uint32_t value)
{            
    uint64_t val = value;
    val *= value;
    uint32_t residue = val % state->prime;
    
    return (value * 2) < state->prime ? residue : state->prime - residue;
}

uint32_t randomseqNext(randomseqState *state)
{       
    uint32_t tmp = (state->index + state->seed1) % state->prime;
    tmp = (tmp + state->seed2) % state->prime;
    uint32_t retval = permuteQPR(state, permuteQPR (state, tmp));
            
    state->index++;
    if (state->index == state->prime)
        randomseqInit(state);        /* Sequence exhausted. Reinitialize. */ 

    if (retval < state->size)
        return retval;
        
    return randomseqNext(state);    
}    
      
#endif