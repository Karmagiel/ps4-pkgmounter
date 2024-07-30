#include "pkg.h"
#include<iostream>
#include<format>
#include<cassert>
#include <fstream>
#include<bit>
#include<vector>
#include<cstring>
#include "pkg_ids.h"

template<typename T>
void inverse_endianess(T& x){
    uint8_t* byte_view = reinterpret_cast<uint8_t*>(&x);
    for(unsigned i = 0; i < sizeof(T)/2; ++i){
        uint8_t tmp = byte_view[i];
        byte_view[i] = byte_view[sizeof(T)-i-1];
        byte_view[sizeof(T)-i-1] = tmp;
    }
}

PKG::PKG(std::string_view filename) {
    //Open PKG File and load header
    m_pkg.open(filename.data(), std::ios::binary);
    if(!m_pkg.good()){
        std::cerr << "Could not open file!\n";
        std::exit(EXIT_FAILURE);
    }

    parse_header();
    parse_files();
}

void PKG::parse_header(){
    static_assert(sizeof(PKG_Header) == PKG_HEADER_SIZE);  // can't do packed... otherwise members of m_header dont bind to type& of inverse_endianess

    if(!m_pkg.read(reinterpret_cast<char*>(&m_header),PKG_HEADER_SIZE)){
        std::cerr << "Error reading!\n";
        std::exit(EXIT_FAILURE);
    }
    assert(m_pkg.gcount() == PKG_HEADER_SIZE);

    //Playstations PKG format in big endian... so on x86_64 we need to change endianess
    if(std::endian::native == std::endian::little) {
        inverse_endianess(m_header.pkg_magic);
        inverse_endianess(m_header.pkg_type);
        inverse_endianess(m_header.pkg_0x008);
        inverse_endianess(m_header.pkg_file_count);
        inverse_endianess(m_header.pkg_entry_count);
        inverse_endianess(m_header.pkg_sc_entry_count);
        inverse_endianess(m_header.pkg_entry_count_2);
        inverse_endianess(m_header.pkg_table_offset);
        inverse_endianess(m_header.pkg_entry_data_size);
        inverse_endianess(m_header.pkg_body_offset);
        inverse_endianess(m_header.pkg_body_size);
        inverse_endianess(m_header.pkg_content_offset);
        inverse_endianess(m_header.pkg_content_size);
        inverse_endianess(m_header.pkg_drm_type);
        inverse_endianess(m_header.pkg_content_type);
        inverse_endianess(m_header.pkg_content_flags);
        inverse_endianess(m_header.pkg_promote_size);
        inverse_endianess(m_header.pkg_version_date);
        inverse_endianess(m_header.pkg_version_hash);
        inverse_endianess(m_header.pkg_0x088);
        inverse_endianess(m_header.pkg_0x08C);
        inverse_endianess(m_header.pkg_0x090);
        inverse_endianess(m_header.pkg_0x094);
        inverse_endianess(m_header.pkg_iro_tag);
        inverse_endianess(m_header.pkg_drm_type_version);
        inverse_endianess(m_header.pfs_image_count);
        inverse_endianess(m_header.pfs_image_flags);
        inverse_endianess(m_header.pfs_image_offset);
        inverse_endianess(m_header.pfs_image_size);
        inverse_endianess(m_header.mount_image_offset);
        inverse_endianess(m_header.mount_image_size);
        inverse_endianess(m_header.pkg_size);
        inverse_endianess(m_header.pfs_signed_size);
        inverse_endianess(m_header.pfs_cache_size);
        inverse_endianess(m_header.pfs_split_size_nth_0);
        inverse_endianess(m_header.pfs_split_size_nth_1);
    }
    assert(m_header.pkg_magic == 0x7F434E54);

    std::cout << std::format("Name: {}, Type: {}, file_count: {}, entry_count: {}, body_offset={:#x}, body_size={:#x}, image_count={}, image_offset={:#x}, image_size={:#x}, mnt_image_offset={:#x}, mnt_image_size={:#x}\n",  std::string_view{reinterpret_cast<char*>(m_header.pkg_content_id), sizeof(m_header.pkg_content_id)},
                             m_header.pkg_type, m_header.pkg_file_count, m_header.pkg_entry_count, m_header.pkg_body_offset, m_header.pkg_body_size, m_header.pfs_image_count, m_header.pfs_image_offset, m_header.pfs_image_size,
                             m_header.mount_image_offset, m_header.mount_image_size) << std::endl;
}

void PKG::parse_files(){
    static_assert(sizeof(PKG_TableEntry) == PKG_TABLEENTRY_SIZE);

    std::vector<PKG_TableEntry> entries;
    std::optional<PKG_TableEntry> filenames_table = std::nullopt;

    //Load file entries
    for(unsigned i = 0; i < m_header.pkg_file_count; ++i){
        uint32_t offset = m_header.pkg_table_offset + i*PKG_TABLEENTRY_SIZE;
        m_pkg.seekg(offset, std::ios::beg);
        PKG_TableEntry entry;
        if(!m_pkg.read(reinterpret_cast<char*>(&entry), PKG_TABLEENTRY_SIZE)){
            std::cerr << "Error reading PKG_TableEntry!\n";
            std::exit(1);
        }
        assert(m_pkg.gcount() == PKG_TABLEENTRY_SIZE);
        if(std::endian::native == std::endian::little){
            inverse_endianess(entry.id);
            inverse_endianess(entry.filename_offset);
            inverse_endianess(entry.flags1);
            inverse_endianess(entry.flags2);
            inverse_endianess(entry.offset);
            inverse_endianess(entry.size);
            //padding skipped
        }
        if(entry.id == 0x200){
            filenames_table = entry;
        }else {
            entries.push_back(entry);
        }
    }

    //Resolve Name for file-entries and put into map
    std::cout << std::format("Found filename table? {}\n", filenames_table.has_value());
    if(filenames_table){
        std::cout << std::format("Filenames-table offset: {:#x}, filename-offset: {:#x}, size={}\n", filenames_table->offset, filenames_table->filename_offset, filenames_table->size);
    }
    for(auto& entry : entries){
        std::string name;
        // lookup name, or use default .id_<id>
        const char* id_name = get_name_for(entry.id);
        name = id_name ? id_name : (".id_"+std::to_string(entry.id));

        // overwrite name
        if(filenames_table){
            m_pkg.seekg(filenames_table->offset + entry.filename_offset);
            // allocate enough storage (well, can max out the table if unlucky)
            auto max_filename_size = filenames_table->size - entry.filename_offset;
            char* buf = reinterpret_cast<char*>(alloca(max_filename_size + 1));
            buf[max_filename_size] = 0;
            // read 0-terminated name
            if(!m_pkg.read(buf, max_filename_size)){
                std::cerr << "Error reading filename\n";
                std::exit(1);
            }
            // only use strings of non-zero length; string will automatically allocate till first \0
            if(*buf)
                name = buf;
        }

        //put into map
        assert(!m_files.contains(name));
        PKG_File file;
        file.id = entry.id;
        file.size = entry.size;
        file.offset = entry.offset;
        file.flags1 = entry.flags1;
        file.flags2 = entry.flags2;
        file.name = std::move(name);
        //m_files.emplace(file.name, std::move(file));

        m_files[file.name] = std::move(file);
    }
    PKG_File pfs;
    pfs.id = 0;
    pfs.size = m_header.pfs_image_size;
    pfs.offset = m_header.pfs_image_offset;
    pfs.flags1 = pfs.flags2 = 0;
    pfs.name = "pfs";
    m_files[pfs.name] = pfs;
}

int PKG::read_file_to_buf(std::string_view filename, char *buf, size_t size, off_t offset){
    std::string filename_str{filename};
    auto it = m_files.find(filename_str);
    if(it == m_files.end())
        return -ENOENT;
    const PKG_File& f = it->second;

    //wrapping size
    if(size >= std::numeric_limits<uint32_t>::max())
        size = 1<<31;

    //check if offset valid
    if(offset >= f.size){
        return EOF;
    }

    if(!m_pkg.seekg(f.offset + offset, std::ios::beg)){
        return EOF;
    }
    uint32_t max_read = std::min(static_cast<uint32_t>(f.size-offset), static_cast<uint32_t>(size));
    if(!m_pkg.read(buf, max_read)){
        return EOF;
    }
    if(m_pkg.gcount() == 0){
        return EOF;
    }
    return m_pkg.gcount();
}

void PKG::pfs(){
}

std::ostream& operator<<(std::ostream& o, const PKG_File& file){
    o << std::format("{:20} ({} bytes)\t @pkg={:#x} {}", file.name, file.size, file.offset, file.isEncrypted()?(file.hasNewEncryptionAlgo()?"(encrypted [new])":"(encrypted)"):"");
    return o;
}