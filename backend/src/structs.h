#ifndef STRUCTS_H
#define STRUCTS_H

#include <cstdint>
#include <cstring>
#include <ctime>

#pragma pack(push, 1)  // ✅ IMPORTANTE: Sin padding entre campos

// Partition - Según enunciado página 11
struct Partition {
    char part_status;       // '0' = no montada, '1' = montada
    char part_type;         // 'P' = primaria, 'E' = extendida
    char part_fit;          // 'B', 'F', 'W'
    int32_t part_start;     // Byte de inicio
    int32_t part_size;      // Tamaño en bytes
    char part_name[16];     // Nombre de la partición
    int32_t part_correlative;  // Correlativo (-1 hasta mount)
    char part_id[4];        // ID de montaje (ej: 301A)
};

// MBR - Según enunciado página 10-11
struct MBR {
    int32_t mbr_tamano;           // Tamaño total del disco en bytes
    time_t mbr_fecha_creacion;    // Fecha y hora de creación
    int32_t mbr_dsk_signature;    // Número random único
    char dsk_fit;                 // Tipo de ajuste del disco (B, F, W)
    Partition mbr_partitions[4];  // 4 particiones (primarias/extendidas)
};

// EBR - Para particiones lógicas (página 12)
struct EBR {
    char part_status;
    char part_fit;
    int32_t part_start;
    int32_t part_size;
    int32_t part_next;      // Byte del siguiente EBR (-1 si no hay)
    char part_name[16];
};

// SuperBlock - EXT2 (página 14)
struct SuperBlock {
    int32_t s_filesystem_type;    // 2 para EXT2
    int32_t s_inodes_count;
    int32_t s_blocks_count;
    int32_t s_free_blocks_count;
    int32_t s_free_inodes_count;
    time_t s_mtime;
    time_t s_umtime;
    int32_t s_mnt_count;
    int32_t s_magic;              // 0xEF53
    int32_t s_inode_s;
    int32_t s_block_s;
    int32_t s_firts_ino;
    int32_t s_first_blo;
    int32_t s_bm_inode_start;
    int32_t s_bm_block_start;
    int32_t s_inode_start;
    int32_t s_block_start;
};

// Inodo - EXT2 (página 15)
struct Inodo {
    int32_t i_uid;
    int32_t i_gid;
    int32_t i_s;
    time_t i_atime;
    time_t i_ctime;
    time_t i_mtime;
    int32_t i_block[15];    // 12 directos, 1 indirecto, 1 doble, 1 triple
    char i_type;            // '0' = carpeta, '1' = archivo
    char i_perm[3];         // Permisos UGO
};

// Contenido de bloque carpeta (página 16)
struct Content {
    char b_name[12];
    int32_t b_inodo;
};

// Bloque Carpeta - 64 bytes (página 16)
struct BloqueCarpeta {
    Content b_content[4];  // 4 * (12 + 4) = 64 bytes
};

// Bloque Archivo - 64 bytes (página 16)
struct BloqueArchivo {
    char b_content[64];
};

// Bloque Apuntadores - 64 bytes (página 17)
struct BloqueApuntador {
    int32_t b_pointers[16];  // 16 * 4 = 64 bytes
};

#pragma pack(pop)  // ✅ Restaurar padding normal

#endif