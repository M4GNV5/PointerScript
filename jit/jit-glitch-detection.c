/*
 * jit-glitch-detection.c - Functions for inserting instruction validations in
 * order to detect glitching attacks.
 *
 * Copyright (C) 2004  Southern Storm Software, Pty Ltd.
 *
 * This file is part of the libjit library.
 *
 * The libjit library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * The libjit library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the libjit library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <assert.h>

#include "jit-bitset.h"
#include "jit-internal.h"
#include "jit/jit-common.h"
#include "jit/jit-dump.h"
#include "jit/jit-insn.h"
#include "jit/jit-type.h"
#include "jit/jit-util.h"
#include "jit/jit-value.h"

/* https://en.wikipedia.org/wiki/Dominator_(graph_theory) */
static void
compute_dominating_blocks(jit_function_t func)
{
	int i;
	int changed = 1;
	jit_block_t block;
	jit_block_t pred;
	_jit_bitset_t tmp;

	_jit_bitset_init(&tmp);
	_jit_bitset_allocate(&tmp, func->builder->block_count);

	block = func->builder->entry_block;
	while(block)
	{
		/* Initialize the dominating blocks bitset */
		_jit_bitset_allocate(&block->dominating_blocks,
			func->builder->block_count);

		/* we start by setting everyone to dominate everyone */
		_jit_bitset_set_all(&block->dominating_blocks); 

		/* Move on to the next block in the function */
		block = block->next;
	}

	block = func->builder->entry_block;

	/* for the first block only itself is a dominator */
	_jit_bitset_clear(&block->dominating_blocks);
	_jit_bitset_set_bit(&block->dominating_blocks, block->index);

	/* this is a fix point algorithm, i.e. we repeat it until nothing
	   changes anymore */
	while(changed)
	{
		block = func->builder->entry_block;
		block = block->next;
		changed = 0;

		while(block)
		{
			/* make sure we have no dominators which do not dominate all
			   predecessors. This is simply achieved by ANDing together the
			   dominator bitsets. We however also need to check if something
			   changed, which is why we need the `tmp` bitset */
			_jit_bitset_copy(&tmp, &block->dominating_blocks);

			for(i = 0; i < block->num_preds; i++)
			{
				pred = block->preds[i]->src;
				_jit_bitset_and(&block->dominating_blocks, &pred->dominating_blocks);
			}

			/* we always dominate ourself, no matter what our preceeding blocks
			   say */
			_jit_bitset_set_bit(&block->dominating_blocks, block->index);

			_jit_bitset_sub(&tmp, &block->dominating_blocks);
			if(!_jit_bitset_empty(&tmp))
			{
				changed = 1;
			}

			block = block->next;
		}
	}

	_jit_bitset_free(&tmp);
}

int
check_for_kill(jit_block_t block, jit_value_t val)
{
	if(val && _jit_bitset_test_bit(&block->var_kills, val->index))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int
check_for_kill_of_dependency(jit_block_t block, jit_insn_t insn)
{
	if((insn->flags & JIT_INSN_DEST_IS_VALUE && check_for_kill(block, insn->dest))
		|| check_for_kill(block, insn->value1)
		|| check_for_kill(block, insn->value2))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void
find_possible_validator_blocks(jit_function_t func, jit_block_t creator,
	jit_insn_t insn, _jit_bitset_t *validators)
{
	jit_block_t block;
	jit_block_t pred;
	int changed = 1;
	int i;

	/* we start with all blocks which are dominated by the creator */
	_jit_bitset_clear(validators);
	block = func->builder->entry_block;
	while(block)
	{
		if(_jit_bitset_is_allocated(&block->dominating_blocks)
			&& _jit_bitset_test_bit(&block->dominating_blocks, creator->index))
		{
			_jit_bitset_set_bit(validators, block->index);
		}
		block = block->next;
	}

	/* blocks which have a predecessor killing insn->dest, value1 or value2
	   cannot validate `insn`. We eliminate these blocks iteratively until no
	   changes occur */
	while(changed)
	{
		changed = 0;
		block = func->builder->entry_block;

		while(block)
		{
			for(i = 0; i < block->num_preds; i++)
			{
				pred = block->preds[i]->src;
				if(pred != creator
					&& _jit_bitset_test_bit(validators, block->index)
					&& check_for_kill_of_dependency(pred, insn))
				{
					_jit_bitset_clear_bit(validators, block->index);
					changed = 1;
				}
			}
			block = block->next;
		}
	}
}

void
add_duplicate_to_insn_list(_jit_insn_list_t *list, jit_block_t creator,
	jit_insn_t insn)
{
	jit_insn_t dup = jit_malloc(sizeof(*dup));
	jit_memcpy(dup, insn, sizeof(*dup));
	_jit_insn_list_add(list, creator, dup);
}

void
prepare_validations(jit_function_t func, jit_block_t creator, jit_insn_t insn,
	jit_block_t block, _jit_bitset_t *validators,
	_jit_bitset_t *placed_validations)
{
	int i;
	int requires_validation;
	jit_block_t succ;

	if(_jit_bitset_test_bit(placed_validations, block->index))
	{
		/* we already placed or are placing the validation in this block */
		return;
	}
	/* prevent infinite recursion */
	_jit_bitset_set_bit(placed_validations, block->index);

	if(check_for_kill_of_dependency(block, insn))
	{
		/* we need to place a validation at the start of this block */
		add_duplicate_to_insn_list(&block->validations_at_start, creator,
			insn);
		return;
	}

	requires_validation = 0;
	for(i = 0; i < block->num_succs; i++)
	{
		succ = block->succs[i]->dst;
		if(!_jit_bitset_test_bit(validators, succ->index))
		{
			requires_validation = 1;
			break;
		}
	}

	if(requires_validation)
	{
		/* one of our successors cannot validate the instruction, we need to
		   validate it here */
		if(block->ends_in_dead)
		{
			add_duplicate_to_insn_list(&block->validations_at_start, creator,
				insn);
		}
		else
		{
			add_duplicate_to_insn_list(&block->validations_at_end, creator,
				insn);
		}
	}
	else
	{
		/* all of our successor can handle the validation, let them do it */
		for(i = 0; i < block->num_succs; i++)
		{
			succ = block->succs[i]->dst;
			prepare_validations(func, creator, insn, succ, validators,
				placed_validations);
		}
	}
}

void
place_validations_from_list(jit_function_t func, jit_block_t block, _jit_insn_list_t curr,
	jit_label_t *validation_fail)
{
	_jit_insn_list_t next;
	jit_insn_t recomputation;
	jit_value_t equality;

	while(curr)
	{
#ifdef _JIT_DEBUG_GLITCH_DETECTION
		printf("Placing validation for: ");
		jit_dump_insn(stdout, func, curr->insn);
		printf("\n");
#endif

		recomputation = _jit_block_add_insn(func->builder->current_block);
		jit_memcpy(recomputation, curr->insn, sizeof(*recomputation));

		recomputation->dest = jit_value_create(func, jit_value_get_type(curr->insn->dest));
		jit_value_ref(func, recomputation->dest);

		equality = jit_insn_eq(func, curr->insn->dest, recomputation->dest);
		jit_insn_branch_if_not(func, equality, validation_fail);

		next = curr->next;
		jit_free(curr->insn);
		jit_free(curr);
		curr = next;
	}
}

void
place_validations(jit_function_t func, jit_block_t block, jit_label_t *validation_fail)
{
	_jit_insn_list_t curr;
	jit_value_t equality;
	jit_block_t new_block;
	jit_block_t old_exit = func->builder->exit_block;

	if(block->validations_at_start)
	{
		func->builder->exit_block = block;

		new_block = _jit_block_create(func);
		_jit_block_attach_before(block, new_block, new_block);
		func->builder->current_block = new_block;

		place_validations_from_list(func, new_block,
			block->validations_at_start, validation_fail);
		block->validations_at_start = 0;

		func->builder->exit_block = old_exit;
	}

	if(block->validations_at_end)
	{
		func->builder->current_block = block;
		place_validations_from_list(func, block,
			block->validations_at_end, validation_fail);
		block->validations_at_end = 0;
	}
}

static jit_glitch_detection_func detection_handler;
static void jit_handle_glitch_detection()
{
	if(detection_handler != NULL)
		detection_handler();

	assert(("Glitch detection triggered", 0));
}

void jit_glitch_detection_set_handler(jit_glitch_detection_func handler)
{
	detection_handler = handler;
}

void _jit_function_generate_glitching_detection(jit_function_t func)
{
	jit_block_t block;
	jit_insn_iter_t iter;
	jit_insn_t insn;
	_jit_bitset_t validators;
	_jit_bitset_t placed_validations;
	jit_label_t validation_fail_target;
	jit_type_t handler_signature;

	_jit_bitset_init(&validators);
	_jit_bitset_init(&placed_validations);
	_jit_bitset_allocate(&validators, func->builder->block_count);
	_jit_bitset_allocate(&placed_validations, func->builder->block_count);

	compute_dominating_blocks(func);
#ifdef _JIT_DEBUG_GLITCH_DETECTION
	jit_dump_function(stdout, func, "(validation target)");
#endif

	block = func->builder->entry_block;
	while(block)
	{
		jit_insn_iter_init(&iter, block);
		while((insn = jit_insn_iter_next(&iter)) != 0)
		{
			if(insn->opcode == JIT_OP_NOP || insn->dest == 0
				|| insn->value1 == 0
				|| insn->value2 == 0
				|| (insn->flags & JIT_INSN_DEST_OTHER_FLAGS) != 0
				|| (insn->flags & JIT_INSN_DEST_IS_VALUE) != 0)
			{
				continue;
			}

			find_possible_validator_blocks(func, block, insn, &validators);

			_jit_bitset_clear(&placed_validations);

#ifdef _JIT_DEBUG_GLITCH_DETECTION
			printf("Preparing the validation of: ");
			jit_dump_insn(stdout, func, insn);
			printf("\n");
#endif
			prepare_validations(func, block, insn, block, &validators,
				&placed_validations);
		}

		block = block->next;
	}

	/* free our temporary bitsets */
	_jit_bitset_free(&validators);
	_jit_bitset_free(&placed_validations);

	validation_fail_target = jit_label_undefined;
	jit_insn_label(func, &validation_fail_target);

	/* call the jit_handle_glitch_detection function when a glitch is
	   detected. This function then in turn calls the custom handler */
	handler_signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void, 0, 0, 0);
	jit_insn_call_native(func,
		"jit_handle_glitch_detection", jit_handle_glitch_detection,
		handler_signature, 0, 0, JIT_CALL_NORETURN
	);

	/* actually place the validations. We need to do this after analysis, as
	   adding validations will add additional blocks and edges which might
	   confuse the computation otherwise */
	block = func->builder->entry_block;
	while(block)
	{
		place_validations(func, block, &validation_fail_target);

		/* free the dominating blocks bitset. By placing the validations we
		   probably added new blocks and made it invalid */
		_jit_bitset_free(&block->dominating_blocks);

		block = block->next;
	}
}
