#pragma once
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <ctime>
#include <vector>
using namespace std;	
#define NAMESIZE	14
#define HELP		1
#define FORMAT		2
#define MKDIR		3
#define RMDIR		4
#define LS			5
#define CD			6
#define CREATE		7
#define OPEN		8
#define CLOSE		9
#define RM			10
#define WRITE		11
#define READ		12
#define NOCMD		13
#define WRITE_L		14
#define EXIT		0
//BLOCK0 32B + 16B
typedef struct USER {
	char user_name[16];
	char password[16];
}User;
struct FCB {
	int isize;
	int fsize;
	unsigned short freetableno;
	unsigned short nfree;
	unsigned short inodetableno;
	unsigned short ninode;
};
//16B Ŀ¼�� 44B inode
typedef struct CATALOG { 
	unsigned short ino;
	char name[NAMESIZE];
}catalog;
typedef struct INODE {
	enum{
		FILE = 0xAA, DIR = 0x55
	};
	enum{
		READ_ONLY = false, READ_WRITE = true
	};
	bool mode;					//1B false-read true-read-write
	unsigned char type;			//1B 0x00-null 0x55-dir 0xAA-file
	unsigned short parent;		//2B
	unsigned int size;			//4B
	unsigned short block;		//2B
	unsigned short offset;		//2B
	time_t create_time;			//8B
	time_t last_change_time;	//8B
	unsigned short addr[8];		//16B
}INode;
typedef struct USEROPEN {
	enum {
		OPENED = true, CLOSED = false
	};
	unsigned short ino;
	bool mode;					//1B open-true close-false
	INode inode;				//4B
	unsigned char *p;
}useropen;

class FileSystem
{
public:
	static FileSystem &getInstance() {
		static FileSystem instance;
		return instance;
	}
	~FileSystem() {
		if (flag)
			exitsys();
	}

	void startsys() {
		char input[128];
		while (1) {
			showPath();
			gets_s(input);
			
			switch (analyse(input))
			{
			case HELP:		help();						break;
			case FORMAT:	format();					break;
			case MKDIR:		mkdir(input);				break;
			case RMDIR:		rmdir(input);				break;
			case LS:		ls();						break;
			case CD:		cd(input);					break;
			case CREATE:	create(input);				break;
			case OPEN:		open(input);				break;
			case CLOSE:		close(atoi(input));			break;
			case RM:		rm(input);					break;
			case WRITE:		write(atoi(input), 's');	break;
			case READ:		read(atoi(input));			break;
			case WRITE_L:	write(atoi(input), 'l');		break;
			case NOCMD:									break;
			case EXIT:		exitsys();					return;
			default:
				printf("Unknow command\n");
			}
		}
	}

private:
	const int BLOCKSIZE = 512;						//���̿��С
	const int SIZE = 1024000;						//������̿ռ��С
	const unsigned short EMPTYCONTRILBLOCK = 16;	//���п���ƿ���
	const unsigned short INODEBLOCK = 170;			//i�ڵ����
	const unsigned short DATABLOCKS = 1813;			//���ݿ���

	unsigned char *myvhard;							//�������
	unsigned char *startp;							//��¼�����������������ʼλ��
	
	User user;
	FCB fcb;
	unsigned short fblock[100];						//��ǰ���д��̿�ջ
	unsigned short finode[100];						//��ǰ����i �ڵ�ջ
	useropen open_list[10];							//�û����ļ��б�
	useropen activity_inode;						//���i �ڵ�
	string current_path;							//��ǰ·��
	bool flag;

	FileSystem() {
		initsys();
	}
	void initsys() {
		flag = true;
		myvhard = (unsigned char *)malloc(sizeof(unsigned char) * SIZE);
		FILE *fp = fopen("myfsys", "rb");
		if (fp) {
			if (!(fread(myvhard, sizeof(unsigned char), SIZE, fp) == SIZE && myvhard[0] == 0xAA)) {
				fclose(fp);
				printf("myfsys is bad, formatting now\n");
				format();
				fp = fopen("myfsys", "wb+");
				if (!fp) {
					printf("create myfsys failed");
					return;
				}
				fwrite(myvhard, sizeof(unsigned char), SIZE, fp);
			}
		}
		else {
			printf("myfsys not exist! Formatting now!\n");
			format();
			fp = fopen("myfsys", "wb+");
			if (!fp) {
					printf("create myfsys failed!\n");
					return;
				}
			fwrite(myvhard, sizeof(unsigned char), SIZE, fp);
		}
		fclose(fp);

		startp = myvhard + (1 + EMPTYCONTRILBLOCK + INODEBLOCK) * BLOCKSIZE;
		activity_inode.ino = 0;
		memcpy(&activity_inode.inode, (INode *)(myvhard + (1 + EMPTYCONTRILBLOCK) * BLOCKSIZE), sizeof(INode));
		memcpy(&user, (User *)(myvhard + 1), sizeof(User));
		memcpy(&fcb, (FCB *)(myvhard + 1 + sizeof(User)), sizeof(FCB));
		memcpy(fblock, (unsigned short *)(myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * (20 + fcb.freetableno)), sizeof(unsigned short) * 100);
		memcpy(finode, (unsigned short *)(myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * fcb.inodetableno), sizeof(unsigned short) * 100);
		for (int i = 0; i < 10; i++) open_list[i].mode = useropen::CLOSED;

		current_path = "root";
	}

	//operation
	void format() {
		unsigned char *tmp = myvhard;
		memset(myvhard, -1, sizeof(unsigned char) * SIZE);
		strcpy(user.user_name, "root");
		strcpy(user.password, "");
		
		fcb.fsize = 1812;
		fcb.isize = 1812;
		fcb.freetableno = 18;
		fcb.inodetableno = 18;
		fcb.nfree = 12;
		fcb.ninode = 12;

		*tmp = 0xAA;
		memcpy(myvhard + 1, (unsigned char *)&user, sizeof(User));
		memcpy(myvhard + 1 + sizeof(User), (unsigned char *)&fcb, sizeof(FCB));
		unsigned short temp[100];
		int cnt = 0, x = 0;
		for (int i = 1812; i > 0; i--) {
			temp[cnt] = i;
			cnt++;
			if (cnt == 100) {
				memcpy(myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * x, (unsigned char *)temp, sizeof(unsigned short) * 100);
				memcpy(myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * (x + 20),(unsigned char *)temp, sizeof(unsigned short) * 100);
				cnt = 0;
				x++;
			}
		}
		memcpy(myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * x, (unsigned char *)temp, sizeof(unsigned short) * 100);
		memcpy(myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * (x + 20), (unsigned char *)temp, sizeof(unsigned short) * 100);
		memcpy(fblock, (unsigned short *)(myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * (20 + fcb.freetableno)), sizeof(unsigned short) * 100);
		memcpy(finode, (unsigned short *)(myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * fcb.inodetableno), sizeof(unsigned short) * 100);

		INode root;
		root.parent = 0;
		root.mode = INode::READ_ONLY;
		root.type = INode::DIR;
		root.addr[0] = 0;
		root.block = 0;
		root.offset = 0;
		root.size = 0;
		time(&(root.create_time));
		root.last_change_time = root.create_time;
		memcpy(myvhard + BLOCKSIZE + EMPTYCONTRILBLOCK * BLOCKSIZE, (unsigned char *)&root, sizeof(INode));
		activity_inode.ino = 0;
		memcpy(&activity_inode.inode, &root, sizeof(INode));

		current_path = "root";
	}
	void showPath() {
		printf("%s>", current_path.data());
	}
	void help() {
		printf("help\t\t\tָ���б�\n");
		printf("format\t\t\t��ʽ������\n");
		printf("mkdir dirname\t\t������Ϊdirname��Ŀ¼\n");
		printf("rmdir dirname\t\tɾ����Ϊdirname��Ŀ¼\n");
		printf("ls\t\t\t��ʾ��ǰĿ¼�������ļ����ļ���\n");
		printf("cd (dirname)\t\t��ʾ��ǰĿ¼�����޸�\n");
		printf("create filename\t\t������Ϊfilename���ļ�\n");
		printf("open filename\t\t����Ϊfilename���ļ� �������ļ�fd\n");
		printf("close fd\t\t�ر�fdΪfd���ļ�\n");
		printf("rm filename\t\tɾ����Ϊfilename���ļ�\n");
		printf("write fd -type text\tдfdΪfd���ļ���textΪҪд�����ݣ�CtrlZ����\n");
		printf("read fd\t\t\t��ȡfdδfd���ļ�������\n");
		printf("exit\t\t\t�ر�ϵͳ(δ������ļ�����ʧ�޸�����)\n");
	}
	void exitsys() {
		FILE *fp = fopen("myfsys", "wb+");
		if (!fp) printf("open myfsys failed");
		else fwrite(myvhard, sizeof(unsigned char), SIZE, fp);
		fclose(fp);
		free(myvhard);
		flag = false;
	}
	
	//dir
	void cd(char *dirname) {
		char name[14];
		unsigned short parent;
		if (!analysisName(dirname, name, parent, INode::DIR)) return;
		unsigned short id = checkExist(parent, name, INode::DIR);
		if (id == 65535) {
			printf("Cannot find the dir\n");
			return;
		}

		activity_inode.ino = id;
		memcpy(&activity_inode.inode, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * id), sizeof(INode));
		
		changePath();
	}
	void mkdir(char *dirname) {
		char name[14];
		unsigned short parent;
		if (!analysisName(dirname, name, parent, INode::DIR)) return;

		if (checkExist(parent, name, INode::DIR) != 65535) {
			printf("%s exists\n", name);
			return;
		}

		int id = iget();
		if (id == -1) {
			printf("inode not enough\n");
			return;
		}

		INode tmp;
		memcpy(&tmp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), sizeof(INode));
		if (tmp.size >= 8 * (BLOCKSIZE / sizeof(catalog))) {
			printf("Too many files in parent dir\n");
			return;
		}
		
		tmp.addr[0] = alloc();
		if (tmp.addr[0] == -1) return;
		time(&(tmp.create_time));
		tmp.last_change_time = tmp.create_time;
		tmp.parent = parent;
		tmp.type = INode::DIR;
		tmp.size = 0;
		tmp.mode = INode::READ_ONLY;
		tmp.block = 0;
		tmp.offset = 0;
		for (int i = 1; i < 8; i++)
			tmp.addr[i] = -1;
		addChild(parent, id, name, tmp.create_time);
		memcpy((myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * id), (char *)&tmp, sizeof(INode));
		if (parent == activity_inode.ino) memcpy(&activity_inode.inode, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), sizeof(INode));
	}
	void rmdir(char *dirname) {
		char name[14];
		unsigned short parent;
		if (!analysisName(dirname, name, parent, INode::DIR)) return;
		int id = checkExist(parent, name, INode::DIR);
		if (id == 65535) {
			printf("Cannot find the dir\n");
			return;
		}
		if (id == 0) {
			printf("Cannot delete root\n");
			return;
		}
		INode tmp;
		memcpy(&tmp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * id), sizeof(INode));
		if (tmp.size) {
			printf("%s not empty, cannot delete\n", name);
			return;
		}

		catalog c, temp;
		bool flag = false;
		memcpy(&tmp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), sizeof(INode));
		if (tmp.offset)
			memcpy(&temp, (catalog *)(startp + tmp.addr[tmp.block] * BLOCKSIZE + (tmp.offset - 1) * sizeof(catalog)), sizeof(catalog));
		else 
			memcpy(&temp, (catalog *)(startp + tmp.addr[tmp.block - 1] * BLOCKSIZE + (31) * sizeof(catalog)), sizeof(catalog));
		for (int i = 0; i <= tmp.block; i++) {
			for (int j = 0; j < BLOCKSIZE; j++) {
				memcpy(&c, (catalog *)(startp + tmp.addr[i] * BLOCKSIZE + j * sizeof(catalog)), sizeof(catalog));
				if (c.ino == id) {
					memcpy((startp + tmp.addr[i] * BLOCKSIZE + j * sizeof(catalog)), (unsigned char *)&temp, sizeof(catalog));
					flag = true;
					break;
				}
			}
			if (flag) break;
		}

		tmp.size--;
		if (!tmp.offset) {
			freeBlock(tmp.addr[tmp.block]);
			tmp.offset = 31;
			tmp.block--;
		}
		tmp.offset--;
		time(&tmp.last_change_time);
		memcpy((myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), (unsigned char *)&tmp, sizeof(INode));

		if (parent == activity_inode.ino) memcpy(&activity_inode.inode, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), sizeof(INode));
		freeInode(id);
	}
	void ls(void) {
		unsigned int cnt= 0;
		catalog c;
		INode tmp;
		if (activity_inode.inode.size) {
			for (int i = 0; i <= activity_inode.inode.block; i++) {
				for (int j = 0; j < 32; j++) {
					if (cnt >= activity_inode.inode.size) return;
					cnt++;
					memcpy(&c, (catalog *)(startp + activity_inode.inode.addr[i] * BLOCKSIZE + j * sizeof(catalog)), sizeof(catalog));
					memcpy(&tmp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * c.ino), sizeof(INode));
					printf("%s\t\t", c.name);
					if (tmp.type == INode::DIR) printf("DIR\n");
					else printf("FILE\n");
					
				}
			}
		}
	}
	
	//file
	int create(char *filename) {
		char name[14];
		unsigned short parent;
		if (!analysisName(filename, name, parent, INode::FILE)) return -1;

		if (checkExist(parent, name, INode::FILE) != 65535) {
			printf("%s exists\n", name);
			return -1;
		}

		int id = iget();
		if (id == -1) {
			printf("i_node is not enough\n");
			return -1;
		}

		INode tmp;
		memcpy(&tmp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), sizeof(INode));
		if (tmp.size >= 8 * (BLOCKSIZE / sizeof(catalog))) {
			printf("Too many files in parent dir\n");
			return -1;
		}
		
		tmp.addr[0] = alloc();
		if (tmp.addr[0] == -1) return -1;
		time(&(tmp.create_time));
		tmp.last_change_time = tmp.create_time;
		tmp.parent = parent;
		tmp.type = INode::FILE;
		tmp.size = 0;
		tmp.mode = INode::READ_WRITE;
		tmp.block = 0;
		tmp.offset = 0;
		for (int i = 1; i < 8; i++)
			tmp.addr[i] = -1;

		addChild(parent, id, name, tmp.create_time);
		memcpy((myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * id), (char *)&tmp, sizeof(INode));
		if (parent == activity_inode.ino)
			memcpy(&activity_inode.inode, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), sizeof(INode));
		return open(filename);
	}
	void rm(char *filename) {
		char name[14];
		unsigned short parent;
		if (!analysisName(filename, name, parent, INode::FILE)) return;
		int id = checkExist(parent, name, INode::FILE);
		if (id == 65535) {
			printf("Cannot find the file\n");
			return;
		}
		if (id == 0) {
			printf("Cannot delete root\n");
			return;
		}
		for (int i = 0; i < 10; i++)
			if (open_list[i].ino == id && open_list[i].mode == useropen::OPENED) {
				printf("Cannot close %s because it is openning and the fd is %d\n", filename, i);
				return;
			}

		INode tmp;
		memcpy(&tmp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * id), sizeof(INode));
		for (int i = 0; i < 8; i++)
			if (tmp.addr[i] != -1)
				freeBlock(tmp.addr[i]);

		catalog c, temp;
		bool flag = false;
		memcpy(&tmp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), sizeof(INode));
		if (tmp.offset)
			memcpy(&temp, (catalog *)(startp + tmp.addr[tmp.block] * BLOCKSIZE + (tmp.offset - 1) * sizeof(catalog)), sizeof(catalog));
		else
			memcpy(&temp, (catalog *)(startp + tmp.addr[tmp.block - 1] * BLOCKSIZE + (31) * sizeof(catalog)), sizeof(catalog));
		for (int i = 0; i <= tmp.block; i++) {
			for (int j = 0; j < BLOCKSIZE; j++) {
				memcpy(&c, (catalog *)(startp + tmp.addr[i] * BLOCKSIZE + j * sizeof(catalog)), sizeof(catalog));
				if (c.ino == id) {
					memcpy((startp + tmp.addr[i] * BLOCKSIZE + j * sizeof(catalog)), (unsigned char *)&temp, sizeof(catalog));
					flag = true;
					break;
				}
			}
			if (flag) break;
		}
		tmp.size--;
		
		if (!tmp.offset) {
			freeBlock(tmp.addr[tmp.block]);
			tmp.offset = 31;
			tmp.block--;
		}
		tmp.offset--;
		time(&tmp.last_change_time);
		memcpy((myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), (unsigned char *)&tmp, sizeof(INode));
		if (parent == activity_inode.ino) memcpy(&activity_inode.inode, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), sizeof(INode));
		freeInode(id);
	}
	int open(char *filename) {
		char name[14];
		unsigned short parent;
		if (!analysisName(filename, name, parent, INode::FILE)) return -1;
		int id = checkExist(parent, name, INode::FILE);
		if (id == 65535) {
			printf("Cannot find the file\n");
			return -1;
		}
		int fd = getFd(id);
		if (fd == -1) return -1;

		memcpy(&open_list[fd].inode, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * id), sizeof(INode));
		open_list[fd].p = startp + open_list[fd].inode.addr[0] * BLOCKSIZE;
		open_list[fd].mode = useropen::OPENED;
		open_list[fd].ino = id;
		int readsize = read(fd);

		printf("%s open successfully and the fd is %d\nSuccessfully read size of file is %d\n", name, fd, readsize);
		return fd;
	}
	void close(int fd) {
		if (!(fd >= 0 && fd < 10)) {
			printf("fd out of size(0 <= fd < 10)\n");
			return;
		}
		if (open_list[fd].mode == useropen::CLOSED) {
			printf("fd has been closed\n");
			return;
		}
		printf("success\n");
		open_list[fd].mode = useropen::CLOSED;
		open_list[fd].p = NULL;
	}
	int write(int fd, const char *text, int len, char wstyle) {
		if (wstyle == 'l') {
			int cnt = 0;
			if (len >= BLOCKSIZE * 8 - 1) {
				printf("Data is too big\n");
				return 0;
			}

			for (int i = 0; i < len / BLOCKSIZE; i++) {
				if (open_list[fd].inode.addr[i] == 65535) {
					open_list[fd].inode.addr[i] = iget();
					if (open_list[fd].inode.addr[i] == 65535) return 0;
				}
				for (int j = 0; j < BLOCKSIZE; j++) {
					if (cnt >= len) break;
					startp[open_list[fd].inode.addr[i] * BLOCKSIZE + j] = text[cnt];
					cnt++;
				}
			}
			if (len % BLOCKSIZE) {
				for (int j = 0; j < BLOCKSIZE; j++) {
					if (cnt >= len) break;
					startp[open_list[fd].inode.addr[len / BLOCKSIZE] * BLOCKSIZE + j] = text[cnt];
					cnt++;
				}
			}

			open_list[fd].inode.size = len;
			open_list[fd].inode.offset = len % BLOCKSIZE;
			open_list[fd].inode.block = len / BLOCKSIZE;
			memcpy((myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * open_list[fd].ino), (unsigned char *)&open_list[fd].inode, sizeof(INode));
			
			return len;
		}

		if (open_list[fd].inode.size + len > BLOCKSIZE * 8 - 1) {
			printf("Data is too big\n");
			return 0;
		}
		int cnt = 0;
		for (int i = 0; i < (len + open_list[fd].inode.size) / BLOCKSIZE; i++) {
			if (open_list[fd].inode.addr[i] == 65535) {
				open_list[fd].inode.addr[i] = iget();
				if (open_list[fd].inode.addr[i] == 65535) return 0;
			}
			for (int j = 0; j < BLOCKSIZE; j++) {
				if (cnt < open_list[fd].inode.size) {
					cnt++;
					continue;
				}
				if (cnt >= len + open_list[fd].inode.size) break;
				startp[open_list[fd].inode.addr[i] * BLOCKSIZE + j] = text[cnt - open_list[fd].inode.size];
				cnt++;
			}
		}

		if ((len + open_list[fd].inode.size) % BLOCKSIZE) {
			for (int j = 0; j < BLOCKSIZE; j++) {
				if (cnt < open_list[fd].inode.size) {
					cnt++;
					continue;
				}
				if (cnt >= (len + open_list[fd].inode.size)) break;

				startp[open_list[fd].inode.addr[(len + open_list[fd].inode.size) / BLOCKSIZE] * BLOCKSIZE + j] = text[cnt - open_list[fd].inode.size];
				cnt++;
			}
		}

		open_list[fd].inode.size = len + open_list[fd].inode.size;
		open_list[fd].inode.offset = (len + open_list[fd].inode.size) % BLOCKSIZE;
		open_list[fd].inode.block = (len + open_list[fd].inode.size) / BLOCKSIZE;
		memcpy((myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * open_list[fd].ino), (unsigned char *)&open_list[fd].inode, sizeof(INode));

		return len;
	}
	int write(int fd, char wstyle) {
		string text;
		char c;
		while ((c = getche()) != 26) {
			text.push_back(c);
		}
		if (wstyle == 'l') {
			if (text.length() > int(BLOCKSIZE) * 8) {
				printf("Data is too big\n");
				return 0;
			}
		}
		else if (wstyle == 's') {
			if (open_list[fd].inode.size + text.length() > BLOCKSIZE * 8) {
				printf("Data is too big\n");
				return 0;
			}
		}
		else {
			printf("Error write style\n");
			return -1;
		}
		return write(fd, text.data(), text.length(), wstyle);
	}
	int read(int fd) {
		if (open_list[fd].mode == USEROPEN::CLOSED) {
			printf("cannot read it, please open a file\n");
			return 0;
		}
		unsigned int len = open_list[fd].inode.size;
		if (len > open_list[fd].inode.size) {
			printf("length out of size\n");
			return 0;
		}

		int blocks = open_list[fd].inode.block;
		unsigned int cnt = 0;
		for (int i = 0; i <= blocks; i++)
			for (int j = 0; j < BLOCKSIZE; j++) {
				if (cnt >= len) break;
				putchar(*(startp + open_list[fd].inode.addr[i] * BLOCKSIZE + j));
				cnt++;
			}
		printf("\n");
		return cnt;
	}
	
	//base
	bool analysisName(char *total, char *local, unsigned short &parent, unsigned char type) {
		char tmp[14] = { '\0' };
		int cnt = 0, i;
		useropen temp;
		catalog c;
		bool flag = true;
		INode t;
		//
		if (total[0] == '/') {
			memcpy(&temp.inode, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK)), sizeof(INode));
			temp.ino = 0;
		}
		else {
			memcpy(&temp.inode, &activity_inode.inode, sizeof(INode));
			temp.ino = activity_inode.ino;
		}
		//
		for (i = 0; i < strlen(total); i++) {
			if (cnt >= 14) {
				printf("Cannot find the path\n");
				return false;
			}
			if (total[i] == '/' && i == strlen(total) - 1) break;
			if (total[i] == '/') {
				flag = false;
				tmp[cnt] = '\0';
				cnt = 0;
				if (!strcmp(tmp, ".") || !strlen(tmp)) {
					flag = true;
					continue;
				}
				else if (!strcmp(tmp, "..")) {
					flag = true;
					if (temp.ino == 0) continue;
					temp.ino = temp.inode.parent;
					memcpy(&temp.inode, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * temp.inode.parent), sizeof(INode));
				}
				else {
					int count = 0;
					for (int j = 0; j <= temp.inode.block; j++) {
						for (int k = 0; k < BLOCKSIZE / sizeof(catalog); k++) {
							if (count >= temp.inode.size || flag) break;
							memcpy(&c, (catalog *)(startp + BLOCKSIZE * temp.inode.addr[j] * BLOCKSIZE + sizeof(catalog) * k), sizeof(catalog));
							if (!strcmp(c.name, tmp)) {
								memcpy(&t, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * c.ino), sizeof(INode));

								if (t.type == INode::DIR) {
									temp.ino = c.ino;
									memcpy(&temp.inode, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK)) + sizeof(INode) * c.ino, sizeof(INode));
									flag = true;
									break;
								}
									
							}

							count++;
						}
						if (count >= temp.inode.size || flag) break;
					}
				}	
			}
			else {
				tmp[cnt] = total[i];
				cnt++;
			}	
		}
		//
		if (!flag) {
			printf("Cannot find the path!\n");
			return false;
		}
		tmp[cnt] = 0;
		strcpy(local, tmp);
		parent = temp.ino;
		return true;
	}
	unsigned short checkExist(unsigned short parent, char *name, unsigned char type) {
		if (!strcmp(name, ".") && type == INode::DIR) return parent;
		INode tmp, temp;
		catalog c;
		unsigned int cnt = 0;
		memcpy(&tmp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), sizeof(INode));
		if (!strcmp(name, "..") && type == INode::DIR) return tmp.parent;
		if (!tmp.size) return -1;
		for (int i = 0; i <= tmp.block; i++)
			for (int j = 0; j < BLOCKSIZE; j++) {
				if (cnt >= tmp.size) return -1;
				memcpy(&c, (catalog *)(startp + tmp.addr[i] * BLOCKSIZE + j * sizeof(catalog)), sizeof(catalog));
				if (!strcmp(c.name, name)) {
					memcpy(&temp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * c.ino), sizeof(INode));
					if (type == temp.type)
						return c.ino;
				}
				cnt++;
			}
		
		return (unsigned short)-1;
	}
	int getFd(unsigned short id) {
		for (int i = 0; i < 10; i++)
			if (open_list[i].mode == useropen::OPENED && open_list[i].ino == id) {
				printf("It has been openned and the fd is %d\n", i);
				return -1;
			}
			
		for (int i = 0; i < 10; i++)
			if (open_list[i].mode == useropen::CLOSED)
				return i;

		printf("Open files list is full\n");
		return -1;
	}
	unsigned short iget() {
		if (!fcb.isize) return -1;
		if (!fcb.ninode) {
			fcb.ninode = 100;
			fcb.inodetableno--;
			memcpy(&finode, (unsigned short *)(myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * fcb.inodetableno), sizeof(unsigned short) * 100);
		}

		fcb.ninode--;
		fcb.isize--;
		memcpy(myvhard + 1 + sizeof(User), (unsigned char *)&fcb, sizeof(FCB));
		return finode[fcb.ninode];
	}
	void freeInode(unsigned short id) {
		if (id == 0) {
			printf("Cannot delete root\n");
			return;
		}
		if (id == -1) return;
		if (fcb.ninode == 100) {
			memcpy((myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * fcb.inodetableno), (unsigned char *)&finode, sizeof(unsigned short) * 100);
			fcb.inodetableno++;
			fcb.ninode = 0;
			fcb.ninode++;
		}
		finode[fcb.nfree] = id;
		fcb.ninode++;
		memcpy(myvhard + 1 + sizeof(User), (unsigned char *)&fcb, sizeof(FCB));
	}
	unsigned short alloc() {
		if (!fcb.fsize) return -1;
		if (!fcb.nfree) {
			fcb.nfree = 100;
			fcb.freetableno--;
			memcpy(fblock, (unsigned short *)(myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * (20 + fcb.freetableno)), sizeof(unsigned short) * 100);
		}

		fcb.nfree--;
		fcb.fsize -= BLOCKSIZE;
		memcpy(myvhard + 1 + sizeof(User), (unsigned char *)&fcb, sizeof(FCB));
		return fblock[fcb.nfree];
	}
	void freeBlock(unsigned short blockno) {
		if (blockno == 0) {
			printf("Canoot delete root\n");
			return;
		}
		if (blockno == 65535) return;
		memset(startp + BLOCKSIZE * blockno, 0, sizeof(unsigned char) * BLOCKSIZE);
		if (fcb.nfree == 100) {
			memcpy((myvhard + BLOCKSIZE + sizeof(unsigned short) * 100 * (20 + fcb.freetableno)), (unsigned char *)&fblock, sizeof(unsigned short) * 100);
			fcb.freetableno++;
			fcb.nfree = 0;
			fcb.fsize += BLOCKSIZE;
		}
		fblock[fcb.nfree] = blockno;
		fcb.nfree++;
		memcpy(myvhard + 1 + sizeof(User), (unsigned char *)&fcb, sizeof(FCB));
	}
	void addChild(unsigned short parent, unsigned short child, char *childname, time_t t) {
		INode tmp;
		catalog c;
		
		c.ino = child;
		strcpy(c.name, childname);
		memcpy(&tmp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), sizeof(INode));
		if (tmp.size >= 8 * (BLOCKSIZE / sizeof(catalog))) {
			printf("Too many files in parent dir\n");
			return;
		}
		
		memcpy((startp + tmp.addr[tmp.block] * BLOCKSIZE + tmp.offset * sizeof(catalog)), (unsigned char *)&c, sizeof(catalog));
		tmp.size++;
		tmp.offset++;
		if (!(tmp.offset % 32) && tmp.size <= 8 * 32) {
			tmp.block++;
			tmp.offset = 0;
			if ((tmp.addr[tmp.block] = alloc()) == -1) {
				printf("alloc spzce failed\n");
				return;
			}
		}
		tmp.last_change_time = t;
		memcpy((myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * parent), (unsigned char *)&tmp, sizeof(INode));
	}
	int analyse(char *input) {
		if (!strlen(input)) return NOCMD;
		if (!strcmp(input, "exit")) return EXIT;
		if (!strcmp(input, "help")) return HELP;
		if (!strcmp(input, "format")) return FORMAT;
		if (!strcmp(input, "ls")) return LS;

		vector<string> split;
		char *d = " ";
		char *p;
		p = strtok(input, d);
		while (p) {
			split.push_back(string(p));
			p = strtok(NULL, d);
		}

		if (!strcmp(split[0].data(), "mkdir")) {
			if (split.size() != 2) return -1;
			strcpy(input, split[1].data());
			return MKDIR;
		}
		if (!strcmp(split[0].data(), "rmdir")) {
			if (split.size() != 2) return -1;
			strcpy(input, split[1].data());
			return RMDIR;
		}
		if (!strcmp(split[0].data(), "cd")) {
			if (split.size() != 2) return -1;
			strcpy(input, split[1].data());
			return CD;
		}
		if (!strcmp(split[0].data(), "create")) {
			if (split.size() != 2) return -1;
			strcpy(input, split[1].data());
			return CREATE;
		}
		if (!strcmp(split[0].data(), "open")) {
			if (split.size() != 2) return -1;
			strcpy(input, split[1].data());
			return OPEN;
		}
		if (!strcmp(split[0].data(), "close")) {
			if (split.size() != 2) return -1;
			strcpy(input, split[1].data());
			return CLOSE;
		}
		if (!strcmp(split[0].data(), "rm")) {
			if (split.size() != 2) return -1;
			strcpy(input, split[1].data());
			return RM;
		}
		if (!strcmp(split[0].data(), "write")) {
			if (split.size() != 3 && split.size() != 2) return -1;
			strcpy(input, split[1].data());
			if (split.size() == 2 || !strcmp(split[2].data(), "-s"))
				return WRITE;
			if (!strcmp(split[2].data(), "-l"))
				return WRITE_L;
			return -1;
		}
		if (!strcmp(split[0].data(), "read")) {
			if (split.size() != 2) return -1;
			strcpy(input, split[1].data());
			return READ;
		}
		return -1;
	}
	void changePath() {
		useropen u;
		memcpy(&u, &activity_inode, sizeof(useropen));
		vector<string> p;
		INode tmp;
		catalog c;
		int cnt;
		bool flag;
		while (u.ino) {
			cnt = 0;
			memcpy(&tmp, (INode *)(myvhard + BLOCKSIZE * (1 + EMPTYCONTRILBLOCK) + sizeof(INode) * u.inode.parent), sizeof(INode));
			flag = false;
			for (int i = 0; i <= tmp.block; i++) {
				for (int j = 0; j < BLOCKSIZE; j++) {
					if (cnt >= tmp.size) return;
					memcpy(&c, (catalog *)(startp + tmp.addr[tmp.block] * BLOCKSIZE + j * sizeof(catalog)), sizeof(catalog));
					if (c.ino == u.ino) {
						p.push_back(string(c.name));
						flag = true;
						break;
					}
					cnt++;
				}
				if (flag) break;
			}
			if (flag) {
				u.ino = u.inode.parent;
				memcpy(&u.inode, &tmp, sizeof(INode));
			}
		}
		current_path = "root";
		for (int i = p.size() - 1; i >= 0; i--) {
			current_path.push_back('\\');
			current_path += p[i];
		}
	}
};