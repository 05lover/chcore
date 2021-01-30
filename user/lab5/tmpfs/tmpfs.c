#include "tmpfs.h"

#include <defs.h>
#include <syscall.h>
#include <string.h>
#include <cpio.h>
#include <launcher.h>

static struct inode *tmpfs_root;

/*
 * Helper functions to calucate hash value of string
 */
static inline u64 hash_chars(const char *str, ssize_t len)
{
	u64 seed = 131;		/* 31 131 1313 13131 131313 etc.. */
	u64 hash = 0;
	int i;

	if (len < 0) {
		while (*str) {
			hash = (hash * seed) + *str;
			str++;
		}
	} else {
		for (i = 0; i < len; ++i)
			hash = (hash * seed) + str[i];
	}

	return hash;
}

/* BKDR hash */
static inline u64 hash_string(struct string *s)
{
	return (s->hash = hash_chars(s->str, s->len));
}

static inline int init_string(struct string *s, const char *name, size_t len)
{
	int i;

	s->str = malloc(len + 1);
	if (!s->str)
		return -ENOMEM;
	s->len = len;

	for (i = 0; i < len; ++i)
		s->str[i] = name[i];
	s->str[len] = '\0';

	hash_string(s);
	return 0;
}

/*
 *  Helper functions to create instances of key structures
 */
static inline struct inode *new_inode(void)
{
	struct inode *inode = malloc(sizeof(*inode));

	if (!inode)
		return ERR_PTR(-ENOMEM);
	
	inode->type = 0;
	inode->size = 0;

	return inode;
}

static struct inode *new_dir(void)
{
	struct inode *inode;

	inode = new_inode();
	if (IS_ERR(inode))
		return inode;
	inode->type = FS_DIR;
	init_htable(&inode->dentries, 1024);

	return inode;
}

static struct inode *new_reg(void)
{
	struct inode *inode;

	inode = new_inode();
	if (IS_ERR(inode))
		return inode;
	inode->type = FS_REG;
	init_radix_w_deleter(&inode->data, free);

	return inode;
}

static struct dentry *new_dent(struct inode *inode, const char *name,
			       size_t len)
{
	struct dentry *dent;
	int err;

	dent = malloc(sizeof(*dent));
	if (!dent)
		return ERR_PTR(-ENOMEM);
	err = init_string(&dent->name, name, len);
	if (err) {
		free(dent);
		return ERR_PTR(err);
	}
	dent->inode = inode;

	return dent;
}

// this function create a file (directory if `mkdir` == true, otherwise regular
// file) and its size is `len`. You should create an inode and corresponding 
// dentry, then add dentey to `dir`'s htable by `htable_add`.
// Assume that no separator ('/') in `name`.
static int tfs_mknod(struct inode *dir, const char *name, size_t len, int mkdir)
{
	struct inode *inode;
	struct dentry *dent;

	BUG_ON(!name);

	if (len == 0) {
	//	WARN("mknod with len of 0");
	//	return -ENOENT;
	}
	// TODO: write your code here
	if(mkdir){
	// directory
		inode = new_dir();
	}
	else{
		inode = new_reg();
	}
	u64 name_len = 0; 
	for(;*(name+name_len) != '\0'&&*(name+name_len) != '/'; ++name_len);
	//printf("mknod: dir: %p, name: %s, len: %d\n", dir, name, name_len);
	dent = new_dent(inode, name, name_len);
	inode->size = len;
	htable_add(&dir->dentries, dent->name.hash, &dent->node);
	return 0;
}

int tfs_mkdir(struct inode *dir, const char *name, size_t len)
{
	return tfs_mknod(dir, name, len, 1 /* mkdir */ );
}

int tfs_creat(struct inode *dir, const char *name, size_t len)
{
	return tfs_mknod(dir, name, len, 0 /* mkdir */ );
}


// look up a file called `name` under the inode `dir` 
// and return the dentry of this file
static struct dentry *tfs_lookup(struct inode *dir, const char *name,
				 size_t len)
{
	u64 hash = hash_chars(name, len);
	struct dentry *dent;
	struct hlist_head *head;

	head = htable_get_bucket(&dir->dentries, (u32) hash);

	for_each_in_hlist(dent, node, head) {
		//printf("[lookup] name:%s, len:%d\n", dent->name.str, dent->name.len);
		if (dent->name.len == len && 0 == strncmp(dent->name.str, name, len))
			return dent;
	}
	return NULL;
}

// Walk the file system structure to locate a file with the pathname stored in `*name`
// and saves parent dir to `*dirat` and the filename to `*name`.
// If `mkdir_p` is true, you need to create intermediate directories when it missing.
// If the pathname `*name` starts with '/', then lookup starts from `tmpfs_root`, 
// else from `*dirat`.
// Note that when `*name` ends with '/', the inode of last component will be
// saved in `*dirat` regardless of its type (e.g., even when it's FS_REG) and
// `*name` will point to '\0'
int tfs_namex(struct inode **dirat, const char **name, int mkdir_p)
{
	BUG_ON(dirat == NULL);
	BUG_ON(name == NULL);
	BUG_ON(*name == NULL);

	//char buff[MAX_FILENAME_LEN + 1];
	struct dentry *dent;
	//int err;

	if (**name == '/') {
		*dirat = tmpfs_root;
		// make sure `name` starts with actual name
		while (**name && **name == '/')
			++(*name);
	} else {
		BUG_ON(*dirat == NULL);
		BUG_ON((*dirat)->type != FS_DIR);
	}
	// make sure a child name exists
	if (!**name)
		return -EINVAL;

	int len = 0;
	while(1){
		len = 0;
		while(**name != '/' && **name != '\0'){
			++(*name);
			++len;
		}	
		if(**name == '\0'){
			*name = *name - len;
			return 0;
		}
		else{
			const char *start = *name - len;
			if((dent = tfs_lookup(*dirat, start, len)) == NULL){
				info("what's missing: %s\n", start);
				if(mkdir_p){
					if(tfs_mkdir(*dirat, start, 0) != 0)
						return -1;
				}
				else{
					return -1;
				}
				dent = tfs_lookup(*dirat, start, len);
			}
			//printf("*dirat: %p, dent->inode: %p\n", *dirat, dent->inode);
			*dirat = dent->inode;
			while(**name == '/')
				++(*name);
		}
	}
}

int tfs_remove(struct inode *dir, const char *name, size_t len)
{
	u64 hash = hash_chars(name, len);
	struct dentry *dent, *target = NULL;
	struct hlist_head *head;

	BUG_ON(!name);

	if (len == 0) {
		WARN("mknod with len of 0");
		return -ENOENT;
	}

	head = htable_get_bucket(&dir->dentries, (u32) hash);

	for_each_in_hlist(dent, node, head) {
		if (dent->name.len == len && 0 == strcmp(dent->name.str, name)) {
			target = dent;
			break;
		}
	}

	if (!target)
		return -ENOENT;

	BUG_ON(!target->inode);

	// remove only when file is closed by all processes
	if (target->inode->type == FS_REG) {
		// free radix tree
		radix_free(&target->inode->data);
		// free inode
		free(target->inode);
		// remove dentry from parent
		htable_del(&target->node);
		// free dentry
		free(target);
	} else if (target->inode->type == FS_DIR) {
		if (!htable_empty(&target->inode->dentries))
			return -ENOTEMPTY;

		// free htable
		htable_free(&target->inode->dentries);
		// free inode
		free(target->inode);
		// remove dentry from parent
		htable_del(&target->node);
		// free dentry
		free(target);
	} else {
		BUG("inode type that shall not exist");
	}

	return 0;
}

int init_tmpfs(void)
{
	tmpfs_root = new_dir();

	return 0;
}

// write memory into `inode` at `offset` from `buf` for length is `size`
// it may resize the file
// `radix_get`, `radix_add` are used in this function
// You can use memory functions defined in libc
ssize_t tfs_file_write(struct inode * inode, off_t offset, const char *data,
		       size_t size)
{
	BUG_ON(inode->type != FS_REG);
	BUG_ON(offset > inode->size);

	u64 page_bound = (inode->size)/PAGE_SIZE;
	
	u64 page_no = offset / PAGE_SIZE;
	u64 page_off = offset % PAGE_SIZE;
	u64 page_end = (offset+size) / PAGE_SIZE;
	void *page;
	if(offset+size > inode->size){
		inode->size = offset+size;
	}
	if(page_no > page_bound){
		for(u64 page_idx = page_bound; page_idx<page_no; page_idx++){
			page = radix_get(&inode->data, page_idx);
			if(page == NULL){
				page = malloc(PAGE_SIZE);
				radix_add(&inode->data, page_idx, page);
			}
		}
	}	
	size_t to_write;
	size_t retsize = size;
	size_t current_offset = 0;
	//printf("[file_write] page_no: %d page_end: %d\n", page_no, page_end);
	for(u64 page_idx = page_no; page_idx<=page_end; ++page_idx){
		page = radix_get(&inode->data, page_idx);
		if(page == NULL){
			page = malloc(PAGE_SIZE);
			radix_add(&inode->data, page_idx, page);
		}
		if(page_idx == page_no){
			to_write = page_off+size<=PAGE_SIZE?size:PAGE_SIZE-page_off;
			size -= to_write;
			memcpy(page+page_off, data+current_offset, to_write);
			current_offset += to_write;
		}
		else{
			to_write = size<PAGE_SIZE?size:PAGE_SIZE;
			size -= to_write;
			memcpy(page, data+current_offset, to_write);
			current_offset += to_write;
		}
	}
	return retsize;
}

// read memory from `inode` at `offset` in to `buf` for length is `size`, do not
// exceed the file size
// `radix_get` is used in this function
// You can use memory functions defined in libc
ssize_t tfs_file_read(struct inode * inode, off_t offset, char *buff,
		      size_t size)
{
	BUG_ON(inode->type != FS_REG);
	BUG_ON(offset >= inode->size);

	size_t retsize = 0;
	size = retsize = size+offset>inode->size?(inode->size-offset):size;

	u64 page_no = offset / PAGE_SIZE;
	u64 page_off = offset % PAGE_SIZE;
	u64 page_end = (offset+size) / PAGE_SIZE;
	size_t to_read = 0;
	size_t current_offset = 0;
	void *page;
	for(u64 page_idx = page_no; page_idx<=page_end; ++page_idx){
		page = radix_get(&inode->data, page_idx);
		if(page == NULL)
			return 0;
		if(page_idx == page_no){
			to_read = page_off+size<PAGE_SIZE?size:PAGE_SIZE-page_off;
			size -= to_read;
			memcpy(buff+current_offset, page+page_off, to_read);
			current_offset += to_read;
		}
		else{
			to_read = size<PAGE_SIZE?size:PAGE_SIZE;
			size -= to_read;
			memcpy(buff+current_offset, page, to_read);
			current_offset += to_read;
		}
	}

	return retsize;
}

// load the cpio archive into tmpfs with the begin address as `start` in memory
// You need to create directories and files if necessary. You also need to write
// the data into the tmpfs.
int tfs_load_image(const char *start)
{
	struct cpio_file *f;
	struct inode *dirat;
	dirat = tmpfs_root; 
	struct dentry *dent;
	const char *leaf;
	ssize_t write_count;

	BUG_ON(start == NULL);

	cpio_init_g_files();
	cpio_extract(start, "/");

	struct inode *reg_file;
	size_t name_len;
	size_t len;
	for (f = g_files.head.next; f; f = f->next) {
		// TODO: Lab5: your code is here
		dirat = tmpfs_root; 
		leaf = f->name;
		//name_len = f->header.c_namesize;
		len = f->header.c_filesize;
		//printf("[load_image] file size:%d\n", len);
		//printf("[load_image] file name:%s\n", f->name);
		if(!tfs_namex(&dirat, &leaf, 1)){
			for(name_len=0;*(leaf+name_len)&&*(leaf+name_len)!='/';name_len++);
			dent = tfs_lookup(dirat, leaf, name_len);
			if(dent == NULL){
				//printf("dirat: %p leaf: %s len: %d\n", dirat, leaf, len);
				if(len != 0){
					tfs_creat(dirat, leaf, len);
					if((dent = tfs_lookup(dirat, leaf, name_len)) != NULL){
						reg_file = dent->inode;
					}
					else{
						return -1;
					}
				}
				else{ //it's a file
					//printf("what's missing:%s\n", leaf);
					tfs_mkdir(dirat, leaf, 0);
					continue;
				}
			}
			else{
				reg_file = dent->inode;
			}
			//printf("[load_image] write to file\n");
			write_count = tfs_file_write(reg_file, 0, f->data, len);
		}
	}
	return 0;
}

static int dirent_filler(void **dirpp, void *end, char *name, off_t off,
			 unsigned char type, ino_t ino)
{
	struct dirent *dirp = *(struct dirent **)dirpp;
	void *p = dirp;
	unsigned short len = strlen(name) + 1 +
	    sizeof(dirp->d_ino) +
	    sizeof(dirp->d_off) + sizeof(dirp->d_reclen) + sizeof(dirp->d_type);
	p += len;
	if (p > end)
		return -EAGAIN;
	dirp->d_ino = ino;
	dirp->d_off = off;
	dirp->d_reclen = len;
	dirp->d_type = type;
	strcpy(dirp->d_name, name);
	*dirpp = p;
	return len;
}

int tfs_scan(struct inode *dir, unsigned int start, void *buf, void *end)
{
	s64 cnt = 0;
	int b;
	int ret;
	ino_t ino;
	void *p = buf;
	unsigned char type;
	struct dentry *iter;

	for_each_in_htable(iter, b, node, &dir->dentries) {
		if (cnt >= start) {
			type = iter->inode->type;
			ino = iter->inode->size;
			ret = dirent_filler(&p, end, iter->name.str,
					    cnt, type, ino);
			if (ret <= 0) {
				return cnt - start;
			}
		}
		cnt++;
	}
	return cnt - start;

}

/* path[0] must be '/' */
struct inode *tfs_open_path(const char *path)
{
	//printf("[tfs_open_path] path: %s\n", path);
	struct inode *dirat = NULL;
	const char *leaf = path;
	struct dentry *dent;
	int err;

	if (*path == '/' && !*(path + 1))
		return tmpfs_root;

	err = tfs_namex(&dirat, &leaf, 0);
	if (err)
		return NULL;

	dent = tfs_lookup(dirat, leaf, strlen(leaf));
	return dent ? dent->inode : NULL;
}
