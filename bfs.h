#ifndef _BFS_H_
#define _BFS_H_

struct bfs_superblock {
};

#define BFS_MAGIC	0x1BADFACE
#define BFS_SUPEROFF	0
#define BFS_DIRSTART	(BFS_SUPEROFF + sizeof(struct bdsuper))
#define BFS_BSIZE	512

#endif /* _BFS_H_ */
