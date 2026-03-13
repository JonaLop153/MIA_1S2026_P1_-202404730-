#ifndef STRUCTS_H
#define STRUCTS_H

#include <cstdint>
#include <cstring>
#include <ctime>

#pragma pack(push, 1)

// MBR - Master Boot Record
struct MBR {
    int32_t mbr_tamano;
    time_t mbr_fecha_creacion;
    int32_t mbr_dsk_signature;
    char dsk_fit;
    struct {
        char part_status;
        char part_type;
        char part_fit;
        int32_t part_start;
        int32_t part_size;
        char part_name[16];
        int32_t part_correlative;
        char part_id[4];
    } mbr_partitions[4];
};

// EBR - Extended Boot Record
struct EBR {
    char part_status;
    char part_fit;
    int32_t part_start;
    int32_t part_size;
    int32_t part_next;
    char part_name[16];
};

// SuperBlock - EXT2
struct SuperBlock {
    int32_t s_filesystem_type;
    int32_t s_inodes_count;
    int32_t s_blocks_count;
    int32_t s_free_blocks_count;
    int32_t s_free_inodes_count;
    time_t s_mtime;
    time_t s_umtime;
    int32_t s_mnt_count;
    int32_t s_magic;
    int32_t s_inode_s;
    int32_t s_block_s;
    int32_t s_firts_ino;
    int32_t s_first_blo;
    int32_t s_bm_inode_start;
    int32_t s_bm_block_start;
    int32_t s_inode_start;
    int32_t s_block_start;
};

// Inodo - EXT2
struct Inodo {
    int32_t i_uid;
    int32_t i_gid;
    int32_t i_s;
    time_t i_atime;
    time_t i_ctime;
    time_t i_mtime;
    int32_t i_block[15];
    char i_type;
    char i_perm[3];
};

// Contenido de bloque carpeta
struct Content {
    char b_name[12];
    int32_t b_inodo;
};

// Bloque Carpeta (64 bytes)
struct BloqueCarpeta {
    Content b_content[4];
};

// Bloque Archivo (64 bytes)
struct BloqueArchivo {
    char b_content[64];
};

// Bloque Apuntadores (64 bytes)
struct BloqueApuntador {
    int32_t b_pointers[16];
};

#pragma pack(pop)

#endif  // ← Esta línea debe estar al final, sin nada después