#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define MAX_LEVEL 5
#define FLAG_MASK 0x8000
#define DATA_MASK 0x7FFF
typedef union Node2 {
	uint16_t x[2];
	uint32_t z;
} Node2;
typedef union Node4 {
	uint16_t x[4];
	Node2 z[2];
	uint64_t y;
} Node4;
typedef union Node8 {
	uint16_t x[8];
	Node2 z[4];
	Node4 y[2];
} Node8;
typedef union Ptr {
	union Ptr *p;
	uint64_t u;
} Ptr;
typedef struct Octree {
	Ptr data;
	uint8_t data_alloc, set_alloc;
	uint16_t data_size, set_size, base;
	Node8 set[];
} Octree;
static_assert(sizeof(Node8)==sizeof(uint16_t[8]), "sizeof(Node8)!=sizeof(uint16_t[8])");
static_assert(sizeof(Ptr)==sizeof(uint64_t), "sizeof(Ptr)!=sizeof(uint64_t)");
static_assert(sizeof(Octree)==sizeof(Node8), "sizeof(Octree)!=sizeof(Node8)");
typedef struct State {
	unsigned level, offset[MAX_LEVEL];
} State;
static unsigned octree_index(const unsigned x, const unsigned z, const unsigned y, const unsigned level) {
	return (x>>level&1)|(z>>level&1)<<1|(y>>level&1)<<2;
}
unsigned octree_get(Octree *const octree, State *const state, const unsigned x, const unsigned z, const unsigned y) {
	unsigned node = octree->base, set = 0;
	for (state->level = MAX_LEVEL;
		state->level && node&FLAG_MASK;
		--state->level,
		node = ((uint16_t *)octree->set)[state->offset[state->level] = (set += node&DATA_MASK)<<3|octree_index(x, z, y, state->level)]);
	return node;
}
static unsigned octree_alloc(const unsigned alloc) {
	const unsigned power = 1<<alloc;
	return (power-1&4681)|power>>1;
}
static unsigned octree_offset(Octree *const octree, const State *const state, const int delta) {
	unsigned set = 0, level = state->level;
	do {
		unsigned offset = state->offset[level];
		while (++offset&7) {
			uint16_t *const node_ptr = (uint16_t *)octree->set+offset;
			if (*node_ptr&FLAG_MASK) {
				if (!set)
					set = (offset>>3)+(*node_ptr&DATA_MASK);
				*node_ptr += delta;
			}
		}
	} while (++level != MAX_LEVEL);
	if (set) {
		Node8 *const set_ptr = octree->set+set;
		memmove(set_ptr+delta, set_ptr, (octree->set_size-set)*sizeof(Node8));
		return set;
	}
	return octree->set_size;
}
Octree *octree_set(Octree *octree, const unsigned x, const unsigned z, const unsigned y, const unsigned new) {
	State state;
	const unsigned node = octree_get(octree, &state, x, z, y);
	if (node != new) {
		if (state.level) {
			const unsigned new_size = octree->set_size+state.level;
			{
				unsigned alloc_size, alloc = octree->set_alloc;
				while ((alloc_size = octree_alloc(alloc)) < new_size)
					++alloc;
				if (octree->set_alloc != alloc) {
					if (!(octree = realloc(octree, alloc_size*sizeof(Node8)+sizeof(Octree))))
						abort();
					octree->set_alloc = alloc;
				}
			}
			Node8 *set_ptr = octree->set;
			if (state.level == MAX_LEVEL)
				octree->base = FLAG_MASK;
			else {
				const unsigned offset = state.offset[state.level], set = octree_offset(octree, &state, state.level);
				((uint16_t *)octree->set)[offset] = set-(offset>>3)|FLAG_MASK;
				set_ptr += set;
			}
			octree->set_size = new_size;
			uint16_t *node_ptr;
			Node4 node_dup;
			node_dup.x[1] = node_dup.x[0] = node;
			node_dup.z[1] = node_dup.z[0];
			for (; set_ptr->y[1] = set_ptr->y[0] = node_dup,
				node_ptr = (uint16_t *)set_ptr+octree_index(x, z, y, --state.level),
				state.level;
				*node_ptr = FLAG_MASK|1, ++set_ptr);
			*node_ptr = new;
		} else {
			uint16_t *node_ptr = (uint16_t *)octree->set+*state.offset;
			if (new&FLAG_MASK)
				*node_ptr = new;
			else {
				unsigned alloc_size;
				Node4 node_dup;
				node_dup.x[1] = node_dup.x[0] = new;
				node_dup.z[1] = node_dup.z[0];
				for (Node8 *set_ptr;
					*node_ptr = new,
					(set_ptr = (Node8 *)((uintptr_t)node_ptr&-sizeof(Node8)))->y[0].y == node_dup.y && set_ptr->y[1].y == node_dup.y;
					node_ptr = (uint16_t *)octree->set+state.offset[state.level])
					if (++state.level == MAX_LEVEL) {
						*octree = (struct Octree){.base = new};
						alloc_size = 0;
						goto dealloc;
					}
				if (state.level) {
					octree_offset(octree, &state, -state.level);
					octree->set_size -= state.level;
					unsigned alloc;
					for (unsigned next_alloc_size, next_alloc = octree->set_alloc;
						alloc = next_alloc,
						(next_alloc_size = octree_alloc(--next_alloc)) >= octree->set_size;
						alloc_size = next_alloc_size);
					if (octree->set_alloc != alloc) {
						octree->set_alloc = alloc;
					dealloc:
						{
							Octree *const prev_octree = realloc(octree, alloc_size*sizeof(Node8)+sizeof(Octree));
							if (prev_octree)
								octree = prev_octree;
						}
					}
				}
			}
		}
	}
	return octree;
}
