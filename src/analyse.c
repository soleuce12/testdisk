/*

    File: analyse.c

    Copyright (C) 1998-2007 Christophe GRENIER <grenier@cgsecurity.org>
  
    This software is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
  
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
  
    You should have received a copy of the GNU General Public License along
    with this program; if not, write the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "types.h"
#include "common.h"
#include "analyse.h"
#include "bfs.h"
#include "bsd.h"
#include "cramfs.h"
#include "exfat.h"
#include "ext2.h"
#include "fat.h"
#include "fatx.h"
#include "hfs.h"
#include "hfsp.h"
#include "jfs_superblock.h"
#include "jfs.h"
#include "hpfs.h"
#include "luks.h"
#include "lvm.h"
#include "md.h"
#include "netware.h"
#include "ntfs.h"
#include "rfs.h"
#include "sun.h"
#include "swap.h"
#include "sysv.h"
#include "ufs.h"
#include "xfs.h"
#include "zfs.h"
#include "log.h"

int search_NTFS_backup(unsigned char *buffer, disk_t *disk, partition_t *partition, const int verbose, const int dump_ind)
{
  void *data=disk->pread_fast(disk, buffer, DEFAULT_SECTOR_SIZE, partition->part_offset);
  if(data==NULL)
      return -1;
  {
    const struct ntfs_boot_sector*ntfs_header=(const struct ntfs_boot_sector*)data;
    /* NTFS recovery using backup sector */
    if(le16(ntfs_header->marker)==0xAA55 &&
	recover_NTFS(disk, ntfs_header, partition, verbose, dump_ind, 1)==0)
      return 1;
  }
  return 0;
}

int search_HFS_backup(unsigned char *buffer, disk_t *disk, partition_t *partition, const int verbose, const int dump_ind)
{
  void *data=disk->pread_fast(disk, buffer, 0x400, partition->part_offset);
  if(data==NULL)
    return -1;
  {
    const hfs_mdb_t *hfs_mdb=(const hfs_mdb_t *)data;
    const struct hfsp_vh *vh=(const struct hfsp_vh *)data;
    /* HFS recovery using backup sector */
    if(hfs_mdb->drSigWord==be16(HFS_SUPER_MAGIC) &&
	recover_HFS(disk, hfs_mdb, partition, verbose, dump_ind, 1)==0)
    {
      strncpy(partition->info,"HFS found using backup sector!",sizeof(partition->info));
      return 1;
    }
    if((be16(vh->version)==4 || be16(vh->version)==5) &&
	recover_HFSP(disk, vh, partition, verbose, dump_ind, 1)==0)
    {
      strncpy(partition->info,"HFS+ found using backup sector!",sizeof(partition->info));
      return 1;
    }
  }
  return 0;
}

int search_EXFAT_backup(unsigned char *buffer, disk_t *disk, partition_t *partition)
{
  void *data=disk->pread_fast(disk, buffer, DEFAULT_SECTOR_SIZE, partition->part_offset);
  if(data==NULL)
    return -1;
  {
    const struct exfat_super_block *exfat_header=(const struct exfat_super_block *)data;
    /* EXFAT recovery using backup sector */
    if(le16(exfat_header->signature)==0xAA55 &&
	recover_EXFAT(disk, exfat_header, partition)==0)
    {
      strncpy(partition->info,"EXFAT found using backup sector!",sizeof(partition->info));
      partition->sb_offset=6*512;
      partition->part_offset-=partition->sb_offset;  /* backup sector */
      return 1;
    }
  }
  return 0;
}

int search_FAT_backup(unsigned char *buffer, disk_t *disk, partition_t *partition, const int verbose, const int dump_ind)
{
  void *data=disk->pread_fast(disk, buffer, DEFAULT_SECTOR_SIZE, partition->part_offset);
  if(data==NULL)
    return -1;
  {
    const struct fat_boot_sector *fat_header=(const struct fat_boot_sector *)data;
    /* FAT32 recovery using backup sector */
    if(le16(fat_header->marker)==0xAA55 &&
	recover_FAT(disk, fat_header, partition, verbose, dump_ind, 1)==0)
    {
      strncpy(partition->info,"FAT found using backup sector!",sizeof(partition->info));
      return 1;
    }
  }
  return 0;
}

int search_type_0(const unsigned char *buffer, disk_t *disk, partition_t *partition, const int verbose, const int dump_ind)
{
  const pv_disk_t *pv=(const pv_disk_t *)buffer;
  const struct disk_fatx *fatx_block=(const struct disk_fatx*)buffer;
  const struct disk_netware *netware_block=(const struct disk_netware *)buffer;
  const struct exfat_super_block *exfat_header=(const struct exfat_super_block *)buffer;
  const struct fat_boot_sector *fat_header=(const struct fat_boot_sector *)buffer;
  const struct luks_phdr *luks=(const struct luks_phdr *)buffer;
  const struct mdp_superblock_1 *sb1=(const struct mdp_superblock_1 *)buffer;
  const struct ntfs_boot_sector*ntfs_header=(const struct ntfs_boot_sector*)buffer;
  const struct xfs_sb *xfs=(const struct xfs_sb *)buffer;
  const union swap_header *swap_header=(const union swap_header *)buffer;
  static const uint8_t LUKS_MAGIC[LUKS_MAGIC_L] = {'L','U','K','S', 0xba, 0xbe};
//  assert(sizeof(union swap_header)<=8*DEFAULT_SECTOR_SIZE);
//  assert(sizeof(pv_disk_t)<=8*DEFAULT_SECTOR_SIZE);
//  assert(sizeof(struct fat_boot_sector)<=8*DEFAULT_SECTOR_SIZE);
//  assert(sizeof(struct ntfs_boot_sector)<=8*DEFAULT_SECTOR_SIZE);
//  assert(sizeof(struct disk_netware)<=8*DEFAULT_SECTOR_SIZE);
//  assert(sizeof(struct xfs_sb)<=8*DEFAULT_SECTOR_SIZE);
//  assert(sizeof(struct disk_fatx)<=8*DEFAULT_SECTOR_SIZE);
  if(verbose>2)
  {
    log_trace("search_type_0 lba=%lu\n",
	(long unsigned)(partition->part_offset/disk->sector_size));
  }
  if(memcmp(swap_header->magic.magic, "SWAP", 4)==0 &&
      recover_Linux_SWAP(swap_header, partition)==0)
    return 1;
  if(memcmp((const char *)pv->id,LVM_ID,sizeof(pv->id)) == 0 &&
      recover_LVM(disk, pv, partition, verbose, dump_ind)==0)
    return 1;
  if(le16(fat_header->marker)==0xAA55 &&
      recover_FAT(disk, fat_header, partition, verbose, dump_ind, 0)==0)
    return 1;
  if(le16(exfat_header->signature)==0xAA55 &&
      recover_EXFAT(disk, exfat_header, partition)==0)
    return 1;
  if(le16(fat_header->marker)==0xAA55 &&
      recover_HPFS(disk, fat_header, partition, verbose)==0)
    return 1;
  if(le16(fat_header->marker)==0xAA55 &&
      recover_OS2MB(disk, fat_header, partition, verbose, dump_ind)==0)
    return 1;
  if(le16(ntfs_header->marker)==0xAA55 &&
      recover_NTFS(disk, ntfs_header, partition, verbose, dump_ind, 0)==0)
    return 1;
  if(memcmp(netware_block->magic, "Nw_PaRtItIoN", 12)==0 &&
      recover_netware(disk, netware_block, partition)==0)
    return 1;
  if (xfs->sb_magicnum==be32(XFS_SB_MAGIC) &&
      recover_xfs(disk, xfs, partition, verbose, dump_ind)==0)
    return 1;
  if(memcmp(fatx_block->magic,"FATX",4)==0 &&
      recover_FATX(fatx_block, partition)==0)
    return 1;
  if(memcmp(luks->magic,LUKS_MAGIC,LUKS_MAGIC_L)==0 &&
      recover_LUKS(disk, luks, partition, verbose, dump_ind)==0)
    return 1;
  /* MD 1.1 */
  if(le32(sb1->major_version)==1 &&
      recover_MD(disk, (const struct mdp_superblock_s*)buffer, partition, verbose, dump_ind)==0)
  {
    partition->part_offset-=le64(sb1->super_offset)*512;
    return 1;
  }
  return 0;
}

int search_type_1(const unsigned char *buffer, disk_t *disk, partition_t *partition, const int verbose, const int dump_ind)
{
  const struct disklabel *bsd_header=(const struct disklabel *)(buffer+0x200);
  const struct disk_super_block*beos_block=(const struct disk_super_block*)(buffer+0x200);
  const struct cramfs_super *cramfs=(const struct cramfs_super *)(buffer+0x200);
  const struct lvm2_label_header *lvm2=(const struct lvm2_label_header *)(buffer+0x200);
  const struct sysv4_super_block *sysv4=(const struct sysv4_super_block *)(buffer+0x200);
  const sun_partition_i386 *sunlabel=(const sun_partition_i386*)(buffer+0x200);
//  assert(sizeof(struct disklabel)<=2*0x200);
//  assert(sizeof(struct disk_super_block)<=0x200);
//  assert(sizeof(struct cramfs_super)<=2*0x200);
//  assert(sizeof(struct sysv4_super_block)<=2*0x200);
//  assert(sizeof(sun_partition_i386)<=2*0x200);
  if(verbose>2)
  {
    log_trace("search_type_1 lba=%lu\n",
	(long unsigned)(partition->part_offset/disk->sector_size));
  }
  if(le32(bsd_header->d_magic) == DISKMAGIC && le32(bsd_header->d_magic2)==DISKMAGIC &&
	recover_BSD(disk, bsd_header, partition, verbose, dump_ind)==0)
    return 1;
  if(beos_block->magic1==le32(SUPER_BLOCK_MAGIC1) &&
      recover_BeFS(disk, beos_block, partition, dump_ind)==0)
    return 1;
  if(cramfs->magic==le32(CRAMFS_MAGIC) &&
    recover_cramfs(disk, cramfs, partition, verbose, dump_ind)==0)
    return 1;
  if((sysv4->s_magic == (signed)le32(0xfd187e20) || sysv4->s_magic == (signed)be32(0xfd187e20)) &&
      recover_sysv(disk, sysv4, partition, verbose, dump_ind)==0)
    return 1;
  if(memcmp((const char *)lvm2->type, LVM2_LABEL, sizeof(lvm2->type)) == 0 &&
      recover_LVM2(disk, (buffer+0x200), partition, verbose, dump_ind)==0)
    return 1;
  if(le32(sunlabel->magic_start) == SUN_LABEL_MAGIC_START &&
      recover_sun_i386(disk, sunlabel, partition, verbose, dump_ind)==0)
    return 1;
  return 0;
}

int search_type_2(const unsigned char *buffer, disk_t *disk, partition_t *partition, const int verbose, const int dump_ind)
{
  const hfs_mdb_t *hfs_mdb=(const hfs_mdb_t *)(buffer+0x400);
  const struct hfsp_vh *vh=(const struct hfsp_vh *)(buffer+0x400);
  const struct ext2_super_block *sb=(const struct ext2_super_block*)(buffer+0x400);
//  assert(sizeof(struct ext2_super_block)<=1024);
//  assert(sizeof(hfs_mdb_t)<=1024);
//  assert(sizeof(struct hfsp_vh)<=1024);
  if(verbose>2)
  {
    log_trace("search_type_2 lba=%lu\n",
	(long unsigned)(partition->part_offset/disk->sector_size));
  }
  if(le16(sb->s_magic)==EXT2_SUPER_MAGIC &&
      recover_EXT2(disk, sb, partition, verbose, dump_ind)==0)
    return 1;
  if(hfs_mdb->drSigWord==be16(HFS_SUPER_MAGIC) &&
      recover_HFS(disk, hfs_mdb, partition, verbose, dump_ind, 0)==0)
    return 1;
  if((be16(vh->version)==4 || be16(vh->version)==5) &&
      recover_HFSP(disk, vh, partition, verbose, dump_ind, 0)==0)
    return 1;
  return 0;
}

int search_type_8(unsigned char *buffer, disk_t *disk,partition_t *partition,const int verbose, const int dump_ind)
{
  void *data;
  if(verbose>2)
  {
    log_trace("search_type_8 lba=%lu\n",
	(long unsigned)(partition->part_offset/disk->sector_size));
  }
  data=disk->pread_fast(disk, buffer, 4096, partition->part_offset + 4096);
  if(data==NULL)
    return -1;
  { /* MD 1.2 */
    const struct mdp_superblock_1 *sb1=(const struct mdp_superblock_1 *)data;
    if(le32(sb1->major_version)==1 &&
        recover_MD(disk, (const struct mdp_superblock_s*)data, partition, verbose, dump_ind)==0)
    {
      partition->part_offset-=(uint64_t)le64(sb1->super_offset)*512-4096;
      return 1;
    }
  }
  return 0;
}

int search_type_16(unsigned char *buffer, disk_t *disk,partition_t *partition,const int verbose, const int dump_ind)
{
  void *data;
  if(verbose>2)
  {
    log_trace("search_type_16 lba=%lu\n",
	(long unsigned)(partition->part_offset/disk->sector_size));
  }
  /* 8k offset */
  data=disk->pread_fast(disk, buffer, 3 * DEFAULT_SECTOR_SIZE, partition->part_offset + 16 * 512);
  if(data==NULL)
    return -1;
  {
    const struct ufs_super_block *ufs=(const struct ufs_super_block *)data;
    const struct vdev_boot_header *zfs=(const struct vdev_boot_header*)data;
    /* Test UFS */
    if((le32(ufs->fs_magic)==UFS_MAGIC || be32(ufs->fs_magic)==UFS_MAGIC ||
	  le32(ufs->fs_magic)==UFS2_MAGIC || be32(ufs->fs_magic)==UFS2_MAGIC) &&
	recover_ufs(disk, ufs, partition, verbose, dump_ind)==0)
      return 1;
    if(le64(zfs->vb_magic)==VDEV_BOOT_MAGIC &&
	recover_ZFS(disk, zfs, partition, verbose, dump_ind)==0)
      return 1;
  }
  return 0;
}

int search_type_64(unsigned char *buffer, disk_t *disk,partition_t *partition,const int verbose, const int dump_ind)
{
  void *data;
  if(verbose>2)
  {
    log_trace("search_type_64 lba=%lu\n",
	(long unsigned)(partition->part_offset/disk->sector_size));
  }
  /* 32k offset */
  data=disk->pread_fast(disk, buffer, 3 * DEFAULT_SECTOR_SIZE, partition->part_offset + 63 * 512);
  if(data==NULL)
    return -1;
  data=(char*)data+0x200;
  {
    const struct jfs_superblock* jfs=(const struct jfs_superblock*)data;
    /* Test JFS */
    if(memcmp(jfs->s_magic,"JFS1",4)==0 &&
	recover_JFS(disk, jfs, partition, verbose, dump_ind)==0)
      return 1;
  }
  return 0;
}

int search_type_128(unsigned char *buffer, disk_t *disk, partition_t *partition, const int verbose, const int dump_ind)
{
  void *data;
  if(verbose>2)
  {
    log_trace("search_type_128 lba=%lu\n",
	(long unsigned)(partition->part_offset/disk->sector_size));
  }
  data=disk->pread_fast(disk, buffer, 11 * DEFAULT_SECTOR_SIZE, partition->part_offset + 126 * 512);
  if(data==NULL)
    return -1;
  data=(char*)data+0x400;
  {
    const struct reiserfs_super_block *rfs=(const struct reiserfs_super_block *)data;
    const struct reiser4_master_sb *rfs4=(const struct reiser4_master_sb *)data;
    const struct ufs_super_block *ufs=(const struct ufs_super_block *)data;
    /* 64k offset */
    /* Test ReiserFS */
    if((memcmp(rfs->s_magic,"ReIs",4) == 0 ||
	  memcmp(rfs4->magic,REISERFS4_SUPER_MAGIC,sizeof(REISERFS4_SUPER_MAGIC)) == 0) &&
	recover_rfs(disk, rfs, partition, verbose, dump_ind)==0)
      return 1;
    /* Test UFS2 */
    if((le32(ufs->fs_magic)==UFS_MAGIC || be32(ufs->fs_magic)==UFS_MAGIC ||
	  le32(ufs->fs_magic)==UFS2_MAGIC || be32(ufs->fs_magic)==UFS2_MAGIC) &&
	recover_ufs(disk, ufs, partition, verbose, dump_ind)==0)
      return 1;
    //  if(recover_gfs2(disk,(buffer+0x400),partition,verbose,dump_ind)==0) return 1;
  }
  return 0;
}
