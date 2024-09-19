/*
 * Lab problem set for UNIX programming course
 * by Chun-Ying Huang <chuang@cs.nctu.edu.tw>
 * License: GPLv2
 */
#include <linux/module.h>	// included for all kernel modules
#include <linux/kernel.h>	// included for KERN_INFO
#include <linux/init.h>		// included for __init and __exit macros
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/errno.h>
#include <linux/sched.h>	// task_struct requried for current_uid()
#include <linux/cred.h>		// for current_uid();
#include <linux/slab.h>		// for kmalloc/kfree
#include <linux/uaccess.h>	// copy_to_user
#include <linux/string.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include "maze.h"

static dev_t devnum;
static struct cdev c_dev;
static struct class *clazz;

// 0 for wall, 1 for path, 2 for current position, 3 for start, 4 for end, 5 for overlapped start and current position
static int maze_cnt = 0;
static int maze_created[3] = {0};
static int maze_x[3] = {0};
static int maze_y[3] = {0};
static int mazes[3][_MAZE_MAXX][_MAZE_MAXY] = {0};

void create_maze(int maze_num, int x, int y) {
	for (int i = 0; i < x; i++) {
		for (int j = 0; j < y; j++) {
			if (i == 1 && j == 1) {
				mazes[maze_num][i][j] = 5;
			} else if (i == 0 || j == 0 || i == x - 1 || j == y - 1) {
				mazes[maze_num][i][j] = 0;
			} else if (i == x - 2 && j == y - 2) {
				mazes[maze_num][i][j] = 4;
			} else {
				int r = get_random_long() % 10;
				if (r < 1) {
					mazes[maze_num][i][j] = 0;
				} else {
					mazes[maze_num][i][j] = 1;
				}
			}
		}
	}
	maze_x[maze_num] = x;
	maze_y[maze_num] = y;
}

int get_maze_for_pid(unsigned int pid) {
	for (int i = 0; i < 3; i++) {
		if (maze_created[i] == pid) {
			return i;
		}
	}
	return -1;
}

maze_info_t get_maze_status(int maze_num) {
	maze_info_t info = {0, 0, 0, 0, 0, 0};
	for (int i = 0; i < maze_x[maze_num]; i++) {
		for (int j = 0; j < maze_y[maze_num]; j++) {
			if (mazes[maze_num][i][j] == 3) {
				info.start_x = i;
				info.start_y = j;
			} else if (mazes[maze_num][i][j] == 4) {
				info.end_x = i;
				info.end_y = j;
			} else if (mazes[maze_num][i][j] == 2) {
				info.cur_x = i;
				info.cur_y = j;
			} else if (mazes[maze_num][i][j] == 5) {
				info.start_x = i;
				info.start_y = j;
				info.cur_x = i;
				info.cur_y = j;
			}
		}
	}
	printk(KERN_INFO "mazemod: maze %d - start: (%d, %d), end: (%d, %d), cur: (%d, %d)\n", maze_num, info.start_x, info.start_y, info.end_x, info.end_y, info.cur_x, info.cur_y);
	return info;
}

bool check_maze_exists(unsigned int pid) {
	for (int i = 0; i < 3; i++) {
		if (maze_created[i] == pid) {
			return true;
		}
	}
	return false;
}

static int mazemod_dev_open(struct inode *i, struct file *f) {
	printk(KERN_INFO "mazemod: device opened.\n");
	return 0;
}

static int mazemod_dev_close(struct inode *i, struct file *f) {
	printk(KERN_INFO "mazemod: device closed.\n");
	int pid = current->pid;
	for (int i = 0; i < 3; i++) {
		if (maze_created[i] == pid) {
			maze_created[i] = 0;
			maze_cnt--;
		}
	}
	return 0;
}

static ssize_t mazemod_dev_read(struct file *f, char __user *buf, size_t len, loff_t *off) {
	printk(KERN_INFO "mazemod: read %zu bytes @ %llu.\n", len, *off);
	unsigned int pid = current->pid;
	int maze_num = get_maze_for_pid(pid);
	if (maze_num < 0) {
		printk(KERN_INFO "mazemod: no maze.\n");
		return -EBADFD;
	}
	// write the maze to buf
	printk(KERN_INFO "mazemod: maze %d (%d x %d) - write to buf.\n", maze_num, maze_x[maze_num], maze_y[maze_num]);
	char* kernel_buf = kmalloc(maze_x[maze_num] * maze_y[maze_num], GFP_KERNEL);
	for (int y = 0; y < maze_y[maze_num]; y++) {
		for (int x = 0; x < maze_x[maze_num]; x++) {
			if (mazes[maze_num][x][y] == 0) {
				kernel_buf[y * maze_x[maze_num] + x] = 1;
			} else {
				kernel_buf[y * maze_x[maze_num] + x] = 0;
			}
			// if (x == 0 || y == 0 || x == maze_x[maze_num] - 1 || y == maze_y[maze_num] - 1) {
			// 	kernel_buf[y * maze_x[maze_num] + x] = 1;
			// } else {
			// 	kernel_buf[y * maze_x[maze_num] + x] = 0;
			// }
			// printk(KERN_INFO "mazemod: %d ", buf[y * maze_x[maze_num] + x]);
		}
	}
	int status = copy_to_user(buf, kernel_buf, maze_x[maze_num] * maze_y[maze_num]);
	kfree(kernel_buf);
	if (status != 0) {
		printk(KERN_INFO "mazemod: copy_to_user failed.\n");
		return -EBUSY;
	}
	return maze_x[maze_num] * maze_y[maze_num];
}

static ssize_t mazemod_dev_write(struct file *f, const char __user *buf, size_t len, loff_t *off) {
	printk(KERN_INFO "mazemod: write %zu bytes @ %llu.\n", len, *off);
	unsigned int pid = current->pid;
	int maze_num = get_maze_for_pid(pid);
	if (maze_num < 0) {
		printk(KERN_INFO "mazemod: no maze.\n");
		return -EBADFD;
	}
	// read an array of coord_t from buf
	coord_t *coord = (coord_t *)buf;
	for (int i = 0; i < len / sizeof(coord_t); i++) {
		printk(KERN_INFO "mazemod: coord %d: (%d, %d).\n", i, coord[i].x, coord[i].y);
		int maze_num = get_maze_for_pid(pid);
		maze_info_t info = get_maze_status(maze_num);
		int cur_x = info.cur_x;
		int cur_y = info.cur_y;
		int new_x = cur_x + coord[i].x;
		int new_y = cur_y + coord[i].y;
		if (mazes[maze_num][new_x][new_y] == 0) {
			continue;
		}
		printk(KERN_INFO "mazemod: move to (%d, %d).\n", new_x, new_y);
		for (int i = 0; i < maze_x[maze_num]; i++) {
			for (int j = 0; j < maze_y[maze_num]; j++) {
				if (mazes[maze_num][i][j] == 2) {
					mazes[maze_num][i][j] = 1;
				}
			}
		}
		mazes[maze_num][new_x][new_y] = 2;
	}
	return len;
}

static long mazemod_dev_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
	unsigned int pid = current->pid;
	printk(KERN_INFO "mazemod: ioctl cmd=%u arg=%lu by %u.\n", cmd, arg, pid);
	if (cmd == MAZE_CREATE) {
		printk(KERN_INFO "mazemod: create maze.\n");
		coord_t *coord = (coord_t *)arg;
		// check if too many mazes
		if (maze_cnt >= 3) {
			printk(KERN_INFO "mazemod: too many mazes.\n");
			return -ENOMEM;
		}
		// check if invalid size
		if (coord->x < 0 || coord->y < 0) {
			printk(KERN_INFO "mazemod: invalid size.\n");
			return -EINVAL;
		}
		// create maze
		printk(KERN_INFO "mazemod: create maze %d x %d.\n", coord->x, coord->y);
		maze_cnt++;
		for (int i = 0; i < 3; i++) {
			if (!maze_created[i]) {
				create_maze(i, coord->x, coord->y);
				maze_created[i] = pid;
				break;
			}
		}
	} else if (cmd == MAZE_RESET) {
		printk(KERN_INFO "mazemod: reset maze.\n");
		if (check_maze_exists(pid)) {
			// reset to starting position
			int maze_num = get_maze_for_pid(pid);
			for (int i = 0; i < _MAZE_MAXX; i++) {
				for (int j = 0; j < _MAZE_MAXY; j++) {
					if (mazes[maze_num][i][j] == 2) {
						mazes[maze_num][i][j] = 0;
					} else if (mazes[maze_num][i][j] == 3) {
						mazes[maze_num][i][j] = 5;
					}
				}
			}
		} else {
			printk(KERN_INFO "mazemod: no maze.\n");
			return -ENOENT;
		}
	} else if (cmd == MAZE_DESTROY) {
		printk(KERN_INFO "mazemod: destroy maze.\n");
		if (check_maze_exists(pid)) {
			// destroy maze
			int maze_num = get_maze_for_pid(pid);
			maze_created[maze_num] = 0;
			maze_cnt--;
		} else {
			printk(KERN_INFO "mazemod: no maze.\n");
			return -ENOENT;
		}
	} else if (cmd == MAZE_GETSIZE) {
		printk(KERN_INFO "mazemod: get maze size.\n");
		if (check_maze_exists(pid)) {
			coord_t *coord = (coord_t *)arg;
			coord->x = maze_x[get_maze_for_pid(pid)];
			coord->y = maze_y[get_maze_for_pid(pid)];
		} else {
			printk(KERN_INFO "mazemod: no maze.\n");
			return -ENOENT;
		}
	} else if (cmd == MAZE_MOVE) {
		printk(KERN_INFO "mazemod: move.\n");
		if (check_maze_exists(pid)) {
			coord_t *coord = (coord_t *)arg;
			int maze_num = get_maze_for_pid(pid);
			maze_info_t info = get_maze_status(maze_num);
			int cur_x = info.cur_x;
			int cur_y = info.cur_y;
			int new_x = cur_x + coord->x;
			int new_y = cur_y + coord->y;
			if (new_x < 0 || new_x >= maze_x[maze_num] || new_y < 0 || new_y >= maze_y[maze_num]) {
				printk(KERN_INFO "mazemod: out of bound.\n");
				return -EINVAL;
			}
			if (mazes[maze_num][new_x][new_y] == 0) {
				printk(KERN_INFO "mazemod: wall.\n");
				return -EINVAL;
			}
			if (mazes[maze_num][new_x][new_y] == 4) {
				printk(KERN_INFO "mazemod: reached end.\n");
				return -EINVAL;
			}
			if (mazes[maze_num][new_x][new_y] == 5) {
				printk(KERN_INFO "mazemod: overlapped start.\n");
				return -EINVAL;
			}
			for (int i = 0; i < maze_x[maze_num]; i++) {
				for (int j = 0; j < maze_y[maze_num]; j++) {
					if (mazes[maze_num][i][j] == 2) {
						mazes[maze_num][i][j] = 1;
					}
				}
			}
			mazes[maze_num][new_x][new_y] = 2;
		} else {
			printk(KERN_INFO "mazemod: no maze.\n");
			return -ENOENT;
		}
	} else if (cmd == MAZE_GETPOS) {
		printk(KERN_INFO "mazemod: get position.\n");
		if (check_maze_exists(pid)) {
			coord_t *coord = (coord_t *)arg;
			int maze_num = get_maze_for_pid(pid);
			maze_info_t info = get_maze_status(maze_num);
			coord->x = info.cur_x;
			coord->y = info.cur_y;
		} else {
			printk(KERN_INFO "mazemod: no maze.\n");
			return -ENOENT;
		}
	} else if (cmd == MAZE_GETSTART) {
		printk(KERN_INFO "mazemod: get start.\n");
		if (check_maze_exists(pid)) {
			coord_t *coord = (coord_t *)arg;
			int maze_num = get_maze_for_pid(pid);
			maze_info_t info = get_maze_status(maze_num);
			coord->x = info.start_x;
			coord->y = info.start_y;
		} else {
			printk(KERN_INFO "mazemod: no maze.\n");
			return -ENOENT;
		}
	} else if (cmd == MAZE_GETEND) {
		printk(KERN_INFO "mazemod: get end.\n");
		if (check_maze_exists(pid)) {
			coord_t *coord = (coord_t *)arg;
			int maze_num = get_maze_for_pid(pid);
			maze_info_t info = get_maze_status(maze_num);
			coord->x = info.end_x;
			coord->y = info.end_y;
		} else {
			printk(KERN_INFO "mazemod: no maze.\n");
			return -ENOENT;
		}
	} else {
		printk(KERN_INFO "mazemod: unknown cmd.\n");
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations mazemod_dev_fops = {
	.owner = THIS_MODULE,
	.open = mazemod_dev_open,
	.read = mazemod_dev_read,
	.write = mazemod_dev_write,
	.unlocked_ioctl = mazemod_dev_ioctl,
	.release = mazemod_dev_close
};

static char proc_buf[10000] = "";
static int mazemod_proc_read(struct seq_file *m, void *v) {
	printk(KERN_INFO "mazemod: proc read.\n");
	memset(proc_buf, 0, sizeof(proc_buf));
	for (int i = 0; i < 3; i++) {
		if (maze_created[i]) {
			char buf_maze[1000] = "";
			int start_x, start_y, end_x, end_y, cur_x, cur_y;
			maze_info_t info = get_maze_status(i);
			start_x = info.start_x;
			start_y = info.start_y;
			end_x = info.end_x;
			end_y = info.end_y;
			cur_x = info.cur_x;
			cur_y = info.cur_y;
			snprintf(buf_maze, sizeof(buf_maze), "#%02d: pid %u - [%d x %d]: (%d, %d) -> (%d, %d) @ (%d, %d)", i, maze_created[i], maze_x[i], maze_y[i], start_x, start_y, end_x, end_y, cur_x, cur_y);
			for (int j = 0; j < maze_x[i]; j++) {
				int cur = 0;
				char buf_row[1000] = "";
				for (int k = 0; k < maze_y[i]; k++) {
					int pos = mazes[i][j][k];
					if (pos == 0) {
						buf_row[cur] = '#';
					} else if (pos == 1) {
						buf_row[cur] = '.';
					} else if (pos == 2) {
						buf_row[cur] = '*';
					} else if (pos == 3) {
						buf_row[cur] = 'S';
					} else if (pos == 4) {
						buf_row[cur] = 'E';
					} else if (pos == 5) {
						buf_row[cur] = '*';
					}
					cur++;
				}
				// buf_row[cur] = '\n';
				buf_row[cur] = '\0';
				snprintf(buf_maze, sizeof(buf_maze), "%s\n- %03d: %s", buf_maze, j, buf_row);
			}
			snprintf(proc_buf, sizeof(proc_buf), "%s\n%s\n", proc_buf, buf_maze);
		} else {
			sprintf(proc_buf, "%s\n#%02d: vacancy\n", proc_buf, i);
		}
	}
	seq_printf(m, proc_buf);
	return 0;
}

static int mazemod_proc_open(struct inode *inode, struct file *file) {
	return single_open(file, mazemod_proc_read, NULL);
}

static const struct proc_ops mazemod_proc_fops = {
	.proc_open = mazemod_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static char *mazemod_devnode(const struct device *dev, umode_t *mode) {
	if(mode == NULL) return NULL;
	*mode = 0666;
	return NULL;
}

static int __init mazemod_init(void)
{
	// create char dev
	if(alloc_chrdev_region(&devnum, 0, 1, "updev") < 0)
		return -1;
	if((clazz = class_create("upclass")) == NULL)
		goto release_region;
	clazz->devnode = mazemod_devnode;
	if(device_create(clazz, NULL, devnum, NULL, "maze") == NULL)
		goto release_class;
	cdev_init(&c_dev, &mazemod_dev_fops);
	if(cdev_add(&c_dev, devnum, 1) == -1)
		goto release_device;

	// create proc
	proc_create("maze", 0, NULL, &mazemod_proc_fops);

	printk(KERN_INFO "mazemod: initialized.\n");
	return 0;    // Non-zero return means that the module couldn't be loaded.

release_device:
	device_destroy(clazz, devnum);
release_class:
	class_destroy(clazz);
release_region:
	unregister_chrdev_region(devnum, 1);
	return -1;
}

static void __exit mazemod_cleanup(void)
{
	remove_proc_entry("maze_mod", NULL);

	cdev_del(&c_dev);
	device_destroy(clazz, devnum);
	class_destroy(clazz);
	unregister_chrdev_region(devnum, 1);

	printk(KERN_INFO "mazemod: cleaned up.\n");
}

module_init(mazemod_init);
module_exit(mazemod_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chun-Ying Huang");
MODULE_DESCRIPTION("The unix programming course demo kernel module.");
