/*
 * Copyright (c) 2011 Frantisek Princ
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup libext4
 * @{
 */ 

/**
 * @file	libext4_inode.c
 * @brief	Ext4 inode structure operations.
 */

#include <byteorder.h>
#include <errno.h>
#include <libblock.h>
#include "libext4.h"

uint32_t ext4_inode_get_mode(ext4_superblock_t *sb, ext4_inode_t *inode)
{
	if (ext4_superblock_get_creator_os(sb) == EXT4_SUPERBLOCK_OS_HURD) {
		return ((uint32_t)uint16_t_le2host(inode->osd2.hurd2.mode_high)) << 16 |
		    ((uint32_t)uint16_t_le2host(inode->mode));
	}
	return uint16_t_le2host(inode->mode);
}

bool ext4_inode_is_type(ext4_superblock_t *sb, ext4_inode_t *inode, uint32_t type)
{
	uint32_t mode = ext4_inode_get_mode(sb, inode);
	return (mode & EXT4_INODE_MODE_TYPE_MASK) == type;
}

/*
uint32_t ext4_inode_get_uid(ext4_inode_t *inode)
*/

uint64_t ext4_inode_get_size(ext4_superblock_t *sb, ext4_inode_t *inode)
{
	uint32_t major_rev = ext4_superblock_get_rev_level(sb);

	if ((major_rev > 0) && ext4_inode_is_type(sb, inode, EXT4_INODE_MODE_FILE)) {
		return ((uint64_t)uint32_t_le2host(inode->size_hi)) << 32 |
			    ((uint64_t)uint32_t_le2host(inode->size_lo));
	}
	return uint32_t_le2host(inode->size_lo);
}

void ext4_inode_set_size(ext4_inode_t *inode, uint64_t value) {
	inode->size_lo = host2uint32_t_le((value << 32) >> 32);
	inode->size_hi = host2uint32_t_le(value >> 32);
}

/*
extern uint32_t ext4_inode_get_access_time(ext4_inode_t *);
extern uint32_t ext4_inode_get_change_inode_time(ext4_inode_t *);
extern uint32_t ext4_inode_get_modification_time(ext4_inode_t *);
extern uint32_t ext4_inode_get_deletion_time(ext4_inode_t *);
extern uint32_t ext4_inode_get_gid(ext4_inode_t *);
*/

uint16_t ext4_inode_get_links_count(ext4_inode_t *inode)
{
	return uint16_t_le2host(inode->links_count);
}

/*
extern uint64_t ext4_inode_get_blocks_count(ext4_inode_t *);
*/

uint32_t ext4_inode_get_flags(ext4_inode_t *inode) {
	return uint32_t_le2host(inode->flags);
}

uint32_t ext4_inode_get_direct_block(ext4_inode_t *inode, uint8_t idx)
{
	assert(idx < EXT4_INODE_DIRECT_BLOCK_COUNT);
	return uint32_t_le2host(inode->blocks[idx]);
}

void ext4_inode_set_direct_block(ext4_inode_t *inode, uint8_t idx, uint32_t fblock)
{
	assert(idx < EXT4_INODE_DIRECT_BLOCK_COUNT);
	inode->blocks[idx] = host2uint32_t_le(fblock);
}

uint32_t ext4_inode_get_indirect_block(ext4_inode_t *inode, uint8_t idx)
{
	assert(idx < EXT4_INODE_INDIRECT_BLOCK_COUNT);
	return uint32_t_le2host(inode->blocks[idx + EXT4_INODE_INDIRECT_BLOCK]);
}

uint32_t ext4_inode_get_extent_block(ext4_inode_t *inode, uint64_t idx, service_id_t service_id)
{
	ext4_extent_header_t *header = ext4_inode_get_extent_header(inode);
	ext4_extent_t *extent;
	ext4_extent_index_t *extent_index;

	uint32_t first_block;
	uint16_t block_count;
	uint64_t phys_block = 0;
	uint64_t child;

	int rc;
	block_t* block = NULL;

	while (ext4_extent_header_get_depth(header) != 0) {

		extent_index = EXT4_EXTENT_FIRST_INDEX(header);

		for (uint16_t i = 0; i < ext4_extent_header_get_entries_count(header); ++i) {
			if(idx >= ext4_extent_index_get_first_block(extent_index)) {

				child = ext4_extent_index_get_leaf(extent_index);

				if (block != NULL) {
					block_put(block);
				}

				rc = block_get(&block, service_id, child, BLOCK_FLAGS_NONE);
				if (rc != EOK) {
					return 0;
				}

				header = (ext4_extent_header_t *)block->data;
				break;
			}
		}
	}

	extent = EXT4_EXTENT_FIRST(header);

	for (uint16_t i = 0; i < ext4_extent_header_get_entries_count(header); ++i) {

		first_block = ext4_extent_get_first_block(extent);
		block_count = ext4_extent_get_block_count(extent);

		if ((idx >= first_block) && (idx < first_block + block_count)) {
			phys_block = ext4_extent_get_start(extent) + idx;
			phys_block -= ext4_extent_get_first_block(extent);

			// Memory leak prevention
			if (block != NULL) {
				block_put(block);
			}
			return phys_block;
		}
		// Go to the next extent
		++extent;
	}


	EXT4FS_DBG("ERROR - reached function end");
	return phys_block;
}

ext4_extent_header_t * ext4_inode_get_extent_header(ext4_inode_t *inode)
{
	return (ext4_extent_header_t *)inode->blocks;
}

// Flags checker
bool ext4_inode_has_flag(ext4_inode_t *inode, uint32_t flag)
{
	if (ext4_inode_get_flags(inode) & flag) {
		return true;
	}
	return false;
}

bool ext4_inode_can_truncate(ext4_superblock_t *sb, ext4_inode_t *inode)
{
	 if (ext4_inode_has_flag(inode, EXT4_INODE_FLAG_APPEND)
			 || ext4_inode_has_flag(inode, EXT4_INODE_FLAG_IMMUTABLE)) {
		 return false;
	 }


	 if (ext4_inode_get_mode(sb, inode) == EXT4_INODE_MODE_FILE
			 || ext4_inode_get_mode(sb, inode) == EXT4_INODE_MODE_DIRECTORY) {
		 return true;
	 }

	 return false;
}

/**
 * @}
 */ 
