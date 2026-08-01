#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mount.h>
#include <unistd.h>
#include <errno.h>

static void bail(){
	printf("dying in 60 seconds\n");
	sleep(60);
	exit(1);
}

static void cd(const char *path){
	int cd_status = chdir(path);
	if (cd_status != 0){
		printf("failed switching to %s, %s\n", path, strerror(errno));
		bail();
	}	
}

static void ch_a_root(const char *path){
	int chroot_status = chroot(path);
	if (chroot_status != 0){
		printf("failed chrooting to %s, %s\n", path, strerror(errno));
		bail();
	}	
}

static void move_mount_to_root(const char *path){
	int mount_status = mount(path, "/", NULL, MS_MOVE, NULL);
	if (mount_status != 0){
		printf("failed overmounting %s to root, %s\n", path, strerror(errno));
		bail();
	}	
}

int main(int argc, char **argv){
	if (argc < 3){
		printf("not enough arguments to switch root\n");
		bail();
	}

	printf("new root: %s\n", argv[1]);

	cd(argv[1]);
	move_mount_to_root(argv[1]);
	ch_a_root(".");
	cd("/");

	printf("execv args: ");
	for(int i = 0;i < argc - 2;i++){
		printf("%s ", argv[2 + i]);
	}
	printf("\n");
	execv(argv[2], &argv[2]);

	printf("execv failed, %s\n", strerror(errno));
	bail();

	return 0;	
}
