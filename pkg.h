#ifndef PS4COMPAT_PKG_H
#define PS4COMPAT_PKG_H
#include <cstdint>
#include <string_view>
#include<string>
#include <unordered_map>
#include <vector>
#include <fstream>

#define PKG_HEADER_SIZE 0x1000
#define PKG_TABLEENTRY_SIZE 0x20
#include <iostream>

struct PKG_Header{
    uint32_t pkg_magic;                      // 0x000 - 0x7F434E54
    uint32_t pkg_type;                       // 0x004
    uint32_t pkg_0x008;                      // 0x008 - unknown field
    uint32_t pkg_file_count;                 // 0x00C
    uint32_t pkg_entry_count;                // 0x010
    uint16_t pkg_sc_entry_count;             // 0x014
    uint16_t pkg_entry_count_2;              // 0x016 - same as pkg_entry_count
    uint32_t pkg_table_offset;               // 0x018 - file table offset
    uint32_t pkg_entry_data_size;            // 0x01C
    uint64_t pkg_body_offset;                // 0x020 - offset of PKG entries
    uint64_t pkg_body_size;                  // 0x028 - length of all PKG entries
    uint64_t pkg_content_offset;             // 0x030
    uint64_t pkg_content_size;               // 0x038
    unsigned char pkg_content_id[0x24];      // 0x040 - packages' content ID as a 36-byte string
    unsigned char pkg_padding[0xC];          // 0x064 - padding
    uint32_t pkg_drm_type;                   // 0x070 - DRM type
    uint32_t pkg_content_type;               // 0x074 - Content type
    uint32_t pkg_content_flags;              // 0x078 - Content flags
    uint32_t pkg_promote_size;               // 0x07C
    uint32_t pkg_version_date;               // 0x080
    uint32_t pkg_version_hash;               // 0x084
    uint32_t pkg_0x088;                      // 0x088
    uint32_t pkg_0x08C;                      // 0x08C
    uint32_t pkg_0x090;                      // 0x090
    uint32_t pkg_0x094;                      // 0x094
    uint32_t pkg_iro_tag;                    // 0x098
    uint32_t pkg_drm_type_version;           // 0x09C

    uint8_t padding_1[0x100-(0x9C+sizeof(uint32_t))]; // ...

    /* Digest table */
    unsigned char digest_entries1[0x20];     // 0x100 - sha256 digest for main entry 1
    unsigned char digest_entries2[0x20];     // 0x120 - sha256 digest for main entry 2
    unsigned char digest_table_digest[0x20]; // 0x140 - sha256 digest for digest table
    unsigned char digest_body_digest[0x20];  // 0x160 - sha256 digest for main table


    uint8_t padding_2[0x404-(0x160+sizeof(digest_body_digest))];    // ...

    uint32_t pfs_image_count;                // 0x404 - count of PFS images
    uint64_t pfs_image_flags;                // 0x408 - PFS flags
    uint64_t pfs_image_offset;               // 0x410 - offset to start of external PFS image
    uint64_t pfs_image_size;                 // 0x418 - size of external PFS image
    uint64_t mount_image_offset;             // 0x420
    uint64_t mount_image_size;               // 0x428
    uint64_t pkg_size;                       // 0x430
    uint32_t pfs_signed_size;                // 0x438
    uint32_t pfs_cache_size;                 // 0x43C
    unsigned char pfs_image_digest[0x20];    // 0x440
    unsigned char pfs_signed_digest[0x20];   // 0x460
    uint64_t pfs_split_size_nth_0;           // 0x480
    uint64_t pfs_split_size_nth_1;           // 0x488


    uint8_t padding_3[0xFE0 - (0x488+sizeof(uint64_t))]; // ...

    unsigned char pkg_digest[0x20];          // 0xFE0
};

struct PKG_TableEntry{
    uint32_t id;               // File ID, useful for files without a filename entry
    uint32_t filename_offset;  // Offset into the filenames table (ID 0x200) where this file's name is located
    uint32_t flags1;           // Flags including encrypted flag, etc
    uint32_t flags2;           // Flags including encryption key index, etc
    uint32_t offset;           // Offset into PKG to find the file
    uint32_t size;             // Size of the file
    uint64_t padding;          // blank padding
};

//a PKG_TableEntry but with the filename resolved
struct PKG_File{
    uint32_t id;               // File ID, useful for files without a filename entry
    std::string name;
    uint32_t flags1;           // Flags including encrypted flag, etc
    uint32_t flags2;           // Flags including encryption key index, etc
    uint32_t offset;           // Offset into PKG to find the file
    uint32_t size;             // Size of the file

    [[nodiscard]] bool isEncrypted() const{
        return (flags1 & 0x80000000) != 0;
    }
    [[nodiscard]] bool hasNewEncryptionAlgo() const{
        return (flags1 & 0x10000000) != 0;
    }
    [[nodiscard]] uint32_t getTableEntryKexIndex() const{
        return (flags2 & 0xF000) >> 12;
    }

};
std::ostream& operator<<(std::ostream& o, const PKG_File& file);

class PKG{
public:
    explicit PKG(std::string_view filename);

    const std::unordered_map<std::string, PKG_File>& getFiles(){ return m_files; }

    int read_file_to_buf(std::string_view filename, char *buf, size_t size, off_t offset);

    void pfs();


private:
    void parse_header();
    void parse_files();

    std::ifstream m_pkg{};
    PKG_Header m_header{};
    std::unordered_map<std::string, PKG_File> m_files{};
};

#endif //PS4COMPAT_PKG_H
