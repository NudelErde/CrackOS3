//
// Created by nudelerde on 26.01.23.
//


#include "data/array.h"
#include "features/math.h"
#include "file/partition.h"
#include "out/log.h"

namespace file {

struct ext4_filesystem : filesystem::implementation {
    weak_ptr<filesystem::implementation> self;
    struct superblock {
    private:
        int32_t s_inodes_count;
        int32_t s_blocks_count_lo;
        int32_t s_r_blocks_count_lo;
        int32_t s_free_blocks_count_lo;
        int32_t s_free_inodes_count;
        int32_t s_first_data_block;
        int32_t s_log_block_size;
        int32_t s_log_cluster_size;
        int32_t s_blocks_per_group;
        int32_t s_clusters_per_group;
        int32_t s_inodes_per_group;
        int32_t s_mtime;
        int32_t s_wtime;
        int16_t s_mnt_count;
        int16_t s_max_mnt_count;
        uint16_t s_magic;
        int16_t s_state;
        int16_t s_errors;
        int16_t s_minor_rev_level;
        int32_t s_lastcheck;
        int32_t s_checkinterval;
        int32_t s_creator_os;
        int32_t s_rev_level;
        int16_t s_def_resuid;
        int16_t s_def_resgid;
        int32_t s_first_ino;
        int16_t s_inode_size;
        int16_t s_block_group_nr;
        int32_t s_feature_compat;
        int32_t s_feature_incompat;
        int32_t s_feature_ro_compat;
        uint8_t s_uuid[16];
        char s_volume_name[16];
        char s_last_mounted[64];
        int32_t s_algorithm_usage_bitmap;
        int8_t s_prealloc_blocks;
        int8_t s_prealloc_dir_blocks;
        int16_t s_reserved_gdt_blocks;
        uint8_t s_journal_uuid[16];
        int32_t s_journal_inum;
        int32_t s_journal_dev;
        int32_t s_last_orphan;
        int32_t s_hash_seed[4];
        int8_t s_def_hash_version;
        int8_t s_jnl_backup_type;
        int16_t s_desc_size;
        int32_t s_default_mount_opts;
        int32_t s_first_meta_bg;
        int32_t s_mkfs_time;
        int32_t s_jnl_blocks[17];
        int32_t s_blocks_count_hi;
        int32_t s_r_blocks_count_hi;
        int32_t s_free_blocks_count_hi;
        int16_t s_min_extra_isize;
        int16_t s_want_extra_isize;
        int32_t s_flags;
        int16_t s_raid_stride;
        int16_t s_mmp_interval;
        int64_t s_mmp_block;
        int32_t s_raid_stripe_width;
        uint8_t s_log_groups_per_flex;
        uint8_t s_checksum_type;
        uint16_t s_reserved_pad;
        uint64_t s_kbytes_written;
        uint32_t s_snapshot_inum;
        uint32_t s_snapshot_id;
        uint64_t s_snapshot_r_blocks_count;
        uint32_t s_snapshot_list;
        int32_t s_error_count;
        int32_t s_first_error_time;
        uint32_t s_first_error_ino;
        uint64_t s_first_error_block;
        uint8_t s_first_error_func[32];
        uint32_t s_first_error_line;
        uint32_t s_last_error_time;
        uint32_t s_last_error_ino;
        uint32_t s_last_error_line;
        uint64_t s_last_error_block;
        uint8_t s_last_error_func[32];
        uint8_t s_mount_opts[64];
        uint32_t s_usr_quota_inum;
        uint32_t s_grp_quota_inum;
        uint32_t s_overhead_blocks;
        uint32_t s_backup_bgs[2];
        uint8_t s_encrypt_algos[4];
        uint8_t s_encrypt_pw_salt[16];
        uint32_t s_lpf_ino;
        uint32_t s_prj_quota_inum;
        uint32_t s_checksum_seed;
        uint8_t s_wtime_hi;
        uint8_t s_mkfs_time_hi;
        uint8_t s_lastcheck_hi;
        uint8_t s_first_error_time_hi;
        uint8_t s_last_error_time_hi;
        uint8_t s_pad[2];
        uint32_t s_reserved[96];
        uint32_t s_checksum;

    public:
        [[nodiscard]] uint64_t blocks_count() const {
            return s_blocks_count_lo | ((int64_t) s_blocks_count_hi << 32);
        }
        [[nodiscard]] uint16_t magic() const {
            return s_magic;
        }
        [[nodiscard]] string volume_name() const {
            return string::from_char_array(s_volume_name, 16);
        }
        [[nodiscard]] int32_t incompatible_features() const {
            return s_feature_incompat;
        }
        [[nodiscard]] uint64_t blocks_per_group() const {
            return s_blocks_per_group;
        }
        [[nodiscard]] uint64_t group_count() const {
            return (blocks_count() + blocks_per_group() - 1) / blocks_per_group();
        }
        [[nodiscard]] size_t group_descriptor_size() const {
            return s_desc_size;
        }
        [[nodiscard]] size_t inodes_per_group() const {
            return s_inodes_per_group;
        }
        [[nodiscard]] size_t block_size() const {
            return 1024 << s_log_block_size;
        }
        [[nodiscard]] size_t inode_size() const {
            return s_inode_size;
        }
    };
    static_assert(sizeof(superblock) == 1024);
    struct block_group_descriptor {
        int32_t bg_block_bitmap;
        int32_t bg_inode_bitmap;
        int32_t bg_inode_table;
        int16_t bg_free_blocks_count_lo;
        int16_t bg_free_inodes_count_lo;
        int16_t bg_used_dirs_count_lo;
        int16_t bg_flags;
        int32_t bg_exclude_bitmap_lo;
        int16_t bg_block_bitmap_csum_lo;
        int16_t bg_inode_bitmap_csum_lo;
        int16_t bg_itable_unused_lo;
        int16_t bg_checksum;
        int32_t bg_block_bitmap_hi;
        int32_t bg_inode_bitmap_hi;
        int32_t bg_inode_table_hi;
        int16_t bg_free_blocks_count_hi;
        int16_t bg_free_inodes_count_hi;
        int16_t bg_used_dirs_count_hi;
        int16_t bg_itable_unused_hi;
        int32_t bg_exclude_bitmap_hi;
        int16_t bg_block_bitmap_csum_hi;
        int16_t bg_inode_bitmap_csum_hi;
        int32_t bg_reserved;

        [[nodiscard]] uint64_t block_bitmap_index() const {
            return bg_block_bitmap | ((int64_t) bg_block_bitmap_hi << 32);
        }
        [[nodiscard]] uint64_t inode_bitmap_index() const {
            return bg_inode_bitmap | ((int64_t) bg_inode_bitmap_hi << 32);
        }
        [[nodiscard]] uint64_t inode_table_index() const {
            return bg_inode_table | ((int64_t) bg_inode_table_hi << 32);
        }
        [[nodiscard]] uint64_t exclude_bitmap_index() const {
            return bg_exclude_bitmap_lo | ((int64_t) bg_exclude_bitmap_hi << 32);
        }
    };
    static_assert(sizeof(block_group_descriptor) == 64);
    struct inode {
    private:
        uint16_t i_mode;
        uint16_t i_uid;
        uint32_t i_size_lo;
        uint32_t i_atime;
        uint32_t i_ctime;
        uint32_t i_mtime;
        uint32_t i_dtime;
        uint16_t i_gid;
        uint16_t i_links_count;
        uint32_t i_blocks_lo;
        uint32_t i_flags;
        uint8_t osd1[4];
        uint8_t i_block[60];
        uint32_t i_generation;
        uint32_t i_file_acl_lo;
        uint32_t i_size_high;
        uint32_t i_obso_faddr;
        uint8_t osd2[12];
        uint16_t i_extra_isize;
        uint16_t i_checksum_hi;
        uint32_t i_ctime_extra;
        uint32_t i_mtime_extra;
        uint32_t i_atime_extra;
        uint32_t i_crtime;
        uint32_t i_crtime_extra;
        uint32_t i_version_hi;
        uint32_t i_projid;

    public:
        [[nodiscard]] bool uses_extent_tree() const {
            return (i_flags & 0x80000) != 0;
        }
        [[nodiscard]] uint8_t* block_data() {
            return i_block;
        }
        [[nodiscard]] size_t get_size() const {
            return i_size_lo | ((size_t) i_size_high << 32);
        }
        [[nodiscard]] uint16_t get_mode() const {
            return i_mode;
        }
    };
    static_assert(sizeof(inode) == 0xA0);
    struct directory_entry {
        uint32_t inode;
        uint8_t flags;
        string name;
    };
    struct ext4_file : file::implementation {
        struct extent_header {
            uint16_t magic;
            uint16_t entries;
            uint16_t max;
            uint16_t depth;
            uint32_t generation;
        };
        static_assert(sizeof(extent_header) == 12);
        struct extent_node {
            uint32_t block;
            uint32_t next_block_lo;
            uint16_t next_block_hi;
            uint16_t unused;
        };
        static_assert(sizeof(extent_node) == 12);
        struct extent_leaf {
            uint32_t block;
            uint16_t len;
            uint16_t start_hi;
            uint32_t start_lo;
        };
        static_assert(sizeof(extent_leaf) == 12);

        uint64_t inode_number{};
        inode inode_data{};
        shared_ptr<filesystem::implementation> fs;
        [[nodiscard]] ext4_filesystem* get_fs() const {
            return (ext4_filesystem*) fs.get();
        }

        ext4_file() = default;
        ext4_file(uint64_t inode_number, inode inode_data, shared_ptr<filesystem::implementation> fs) : inode_number(inode_number), inode_data(inode_data), fs(std::move(fs)) {}

        void read_region(void* extent_data, void* buffer, size_t offset, size_t size) const {
            auto* header = static_cast<extent_header*>(extent_data);
            if (header->magic != 0xF30A) {
                panic("invalid extent header");
            }
            if (header->depth == 0) {
                // leaf
                auto* array = reinterpret_cast<extent_leaf*>(header + 1);
                for (size_t i = 0; i < header->entries; ++i) {
                    auto& leaf = array[i];
                    auto start_offset_in_file = leaf.block * get_fs()->block_size;
                    auto end_offset_in_file = start_offset_in_file + leaf.len * get_fs()->block_size;
                    auto common_start = max(start_offset_in_file, offset);
                    auto common_end = min(end_offset_in_file, offset + size);
                    if (common_start >= common_end) {
                        continue;
                    }

                    uint64_t block = leaf.start_lo | ((uint64_t) leaf.start_hi << 32);
                    block *= get_fs()->block_size;

                    get_fs()->part.read(buffer, common_start + block, common_end - common_start);
                }
            } else {
                // node
                auto* array = reinterpret_cast<extent_node*>(header + 1);
                panic("deep extent tree not implemented");
            }
        }

        [[nodiscard]] bool is_directory() const {
            return (inode_data.get_mode() & 0x4000) != 0;
        }

        [[nodiscard]] bool is_file() const {
            return (inode_data.get_mode() & 0x8000) != 0;
        }

        template<typename Callable>
        void iterate_directory(Callable c) {
            if (!is_directory()) {
                panic("not a directory");
            }
            unique_ptr<uint8_t> buffer(new uint8_t[get_fs()->block_size]);
            for (uint64_t i = 0; i < get_size(); i += get_fs()->block_size) {
                read(buffer.get(), i, get_fs()->block_size);
                for (size_t offset = 0; offset < get_fs()->block_size;) {
                    struct entry {
                        uint32_t inode;
                        uint16_t size;
                        uint8_t name_length;
                        uint8_t type;
                    };
                    auto e = reinterpret_cast<entry*>(buffer.get() + offset);
                    offset += e->size;
                    if (e->inode == 0) {
                        continue;
                    }
                    directory_entry de{};
                    de.inode = e->inode;
                    de.flags = e->type;
                    de.name = string::from_char_array(reinterpret_cast<char*>(e + 1), e->name_length);
                    if (!c(de)) {
                        return;
                    }
                }
            }
        }

        size_t read(void* buffer, size_t offset, size_t size) override {
            if (!inode_data.uses_extent_tree()) {
                panic("not extent tree");
            }
            read_region(inode_data.block_data(), buffer, offset, size);
            return size;
        }

        size_t get_size() override {
            return inode_data.get_size();
        }

        file::type_t get_type() override {
            if (is_directory()) {
                return file::type_t::directory;
            } else if (is_file()) {
                return file::type_t::normal;
            } else {
                return file::type_t::extra;
            }
        }

        size_t get_directory_entry_count() override {
            size_t count = 0;
            iterate_directory([&](const directory_entry& de) {
                count += 1;
                return true;
            });
            return count;
        }

        size_t get_directory_entries(file::directory_entry* buffer, size_t offset, size_t size) override {
            size_t i = 0;
            iterate_directory([&](const directory_entry& de) {
                if (i >= size) {
                    return false;
                }
                if (i >= offset) {
                    buffer[i - offset].name = de.name;
                    if (de.flags == 2) {
                        buffer[i - offset].type = file::type_t::directory;
                    } else if (de.flags == 1) {
                        buffer[i - offset].type = file::type_t::normal;
                    } else {
                        buffer[i - offset].type = file::type_t::extra;
                    }
                }
                i += 1;
                return true;
            });
            return i;
        }
    };

    partition part;
    string name;
    array<block_group_descriptor> block_groups;
    size_t block_size;
    bool valid;

    unique_ptr<superblock> sb;
    explicit ext4_filesystem(partition p) : part(std::move(p)) {
        valid = false;
        sb = new superblock;
        part.read(sb.get(), 1024, sizeof(superblock));
        name = sb->volume_name();

        Log::debug("ext4", "volume name: %s\n", name);

        // check magic number
        if (sb->magic() != 0xEF53) {
            Log::error("ext4", "invalid magic number: %x\n", sb->magic());
            return;
        }

        // check features
        int32_t incompat = sb->incompatible_features();
        int32_t supported_incompat_features = 0x2 | 0x40 | 0x80 | 0x200;// directory entry has file type, extents, 64 bit, flex block group
        if (incompat != supported_incompat_features) {
            Log::error("ext4", "unsupported incompat features: %x, expected %x\n", incompat, supported_incompat_features);
            return;
        }

        block_size = sb->block_size();
        auto group_count = sb->group_count();
        auto group_desc_size = sb->group_descriptor_size();
        Log::debug("ext4", "block size: %d, group count: %d, group descriptor size: %d\n", block_size, group_count, group_desc_size);
        block_groups = array<block_group_descriptor>(group_count, group_desc_size);
        part.read(block_groups.data(), block_size, group_count * group_desc_size);
        valid = true;
    }

    inode get_inode(uint64_t inode_number) {
        auto group_index = (inode_number - 1) / sb->inodes_per_group();
        auto& group = block_groups[group_index];
        auto inode_table_offset = group.inode_table_index() * block_size;
        auto inode_index = (inode_number - 1) % sb->inodes_per_group();
        auto inode_offset = inode_table_offset + inode_index * sb->inode_size();
        inode result{};
        part.read(&result, inode_offset, min(sizeof(inode), sb->inode_size()));
        return result;
    }

    file open(const char* path) override {
        const char* remaining = path;
        uint64_t inode_number = 2;
        inode current_inode = get_inode(inode_number);
        ext4_file current_file;
        while (true) {
            if (remaining[0] == '/') {
                remaining++;
                continue;
            }
            current_file = ext4_file(inode_number, current_inode, self.lock());
            if (remaining[0] == '\0') {
                return file{new ext4_file(current_file)};
            }
            if (!current_file.is_directory()) {
                return file{nullptr};
            }
            size_t size = 0;
            for (const char* cpy = remaining; *cpy != '\0' && *cpy != '/'; cpy++) {
                size++;
            }
            bool found = false;
            current_file.iterate_directory([&](const directory_entry& de) -> bool {
                if (de.name.length == size && memcmp(de.name.data, remaining, size) == 0) {
                    inode_number = de.inode;
                    current_inode = get_inode(inode_number);
                    remaining += size;
                    found = true;
                    return false;
                } else {
                    return true;
                }
            });
            if (!found) {
                return file{nullptr};
            }
        }
    }
};

shared_ptr<filesystem::implementation> create_ext4_filesystem(const partition& part) {
    auto* ext4_fs = new ext4_filesystem(part);
    if(ext4_fs->valid == false) {
        delete ext4_fs;
        return nullptr;
    }
    shared_ptr<filesystem::implementation> result(static_cast<filesystem::implementation*>(ext4_fs));
    ext4_fs->self = result;
    return result;
}

}// namespace file