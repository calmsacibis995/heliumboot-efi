#ifndef _VNODE_H_
#define _VNODE_H_

#include <efi.h>
#include <efilib.h>

/*
 * vnode types.  VNON means no type.  These values are unrelated to
 * values in on-disk inodes.
 */
typedef enum vtype {
	VNON	= 0,
	VREG	= 1,
	VDIR	= 2
} vtype_t;

/*
 * A minimal vnode structure.
 */
typedef struct vnode {
    vtype_t type;			/* type of vnode */
	void *fs_private;		/* filesystem-specific data */
	void *mount;			/* pointer to mount structure */
} vnode_t;

#endif /* _VNODE_H_ */
