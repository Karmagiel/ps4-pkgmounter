/*
  ADOPTED FROM:  https://github.com/libfuse/libfuse/blob/a6a219f5344a5c09cec34416818342ac220a0df2/example/hello.c
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Compile with:  gcc -Wall hello.c `pkg-config fuse3 --cflags --libs` -o hello
*/
#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include "pkg.h"
#include <memory>
#include <iostream>

/* ~~~~~ Command line options (free-d by fuse) ~~~~ */
static struct options {
    const char *pkgfile;
    int show_help;
} options;
#define OPTION(t, p) { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
        OPTION("--pkgfile=%s", pkgfile),
        OPTION("-h", show_help),
        OPTION("--help", show_help),
        FUSE_OPT_END
};
#undef OPTION
static void show_help(const char *progname){
    printf("usage: %s [options] <mountpoint>\n\n", progname);
    printf("File-system specific options:\n"
           "    --pkgfile=<s>       Path to .pkg file\n"
           "\n");
}



/* ~~~~ FILESYSTEM ~~~~ */
std::unique_ptr<PKG> pkg;

void* fs_init([[maybe_unused]] struct fuse_conn_info *conn, struct fuse_config *cfg){
    cfg->kernel_cache = 1;
    return nullptr;
}

int fs_getattr(const char *path, struct stat *stbuf, [[maybe_unused]] struct fuse_file_info *fi)
{
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    } else if (pkg->getFiles().contains(path+1)) {
        const PKG_File& file = pkg->getFiles().at(path+1);
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = file.size;
        return 0;
    } else {
        return -ENOENT;
    }
}

int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, [[maybe_unused]] off_t offset, [[maybe_unused]] struct fuse_file_info *fi, [[maybe_unused]] enum fuse_readdir_flags flags)
{
    //only support read on top-leveldir (no nested dirs)
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    //we load whole directory in one go => offset always 0;  dont use flags
    const off_t our_off = 0;
    auto our_flags = static_cast<fuse_fill_dir_flags>(0);

    filler(buf, ".", nullptr, our_off, our_flags);
    filler(buf, "..", nullptr, our_off, our_flags);
    for(auto& [name, pkg_file] : pkg->getFiles()){
        filler(buf, name.c_str(), nullptr, our_off, our_flags);
    }

    return 0;
}

int fs_open(const char *path, struct fuse_file_info *fi)
{
    // do we have file?
    if (!pkg->getFiles().contains(path+1))
        return -ENOENT;
    // do we access it readonly?
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;
    return 0;
}

int fs_read(const char *path, char *buf, size_t size, off_t offset, [[maybe_unused]] struct fuse_file_info *fi)
{
    // do we have file?
    if (!pkg->getFiles().contains(path+1))
        return -ENOENT;

    // load from buf
    return pkg->read_file_to_buf(path+1, buf, size, offset);
}


// define for supported instructions
static const struct fuse_operations fs_operations = {
        .getattr	= fs_getattr,
        .open		= fs_open,
        .read		= fs_read,
        .readdir	= fs_readdir,
        .init       = fs_init,
};


int main(int argc, char *argv[])
{
    /* Parse options */
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    options.pkgfile = nullptr;
    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
        return 1;

    /* When --help is specified, first print our own file-system specific help text, then signal fuse_main to show addi-
     * tional help (by adding `--help` to the options again) without usage: line (by setting argv[0] to the empty string) */
    if (options.show_help) {
        show_help(argv[0]);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0][0] = '\0';
    }


    constexpr bool debug = false;
    if(debug){
        [[maybe_unused]] const char* ign = "public_samples/ign.pkg";
        [[maybe_unused]] const char* netflix = "public_samples/netflix.pkg";
        pkg->pfs();
        return 0;
    }else {
        /* Load our PKG-File, exit if not found */
        if (!options.pkgfile) {
            std::cerr << "Please provide .pkg file via --pkgfile.\n";
            return 1;
        }
        pkg = std::make_unique<PKG>(options.pkgfile);
        //print files
        for(const auto& f : pkg->getFiles()){
            std::cout << "\t" << f.second << "\n";
        }
    }

    /* run fuse service in background. */
    int ret = fuse_main(args.argc, args.argv, &fs_operations, nullptr);
    fuse_opt_free_args(&args);
    return ret;
}