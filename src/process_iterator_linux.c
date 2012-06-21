static int get_boot_time()
{
	int uptime;
	FILE *fp = fopen ("/proc/uptime", "r");
	if (fp != NULL)
	{
		char buf[BUFSIZ];
		char *b = fgets(buf, BUFSIZ, fp);
		if (b == buf)
		{
			char *end_ptr;
			double upsecs = strtod(buf, &end_ptr);
			uptime = (int)upsecs;
		}
		fclose (fp);
	}
	time_t now = time(NULL);
	return now - uptime;
}

int init_process_iterator(struct process_iterator *it, struct process_filter *filter)
{
	//open a directory stream to /proc directory
	if ((it->dip = opendir("/proc")) == NULL)
	{
		perror("opendir");
		return -1;
	}
	it->filter = filter;
	it->boot_time = get_boot_time();
	return 0;
}

static int read_process_info(pid_t pid, struct process *p)
{
	static char buffer[1024];
	static char statfile[32];
	static char exefile[1024];
	p->pid = pid;
	//read stat file
	sprintf(statfile, "/proc/%d/stat", p->pid);
	FILE *fd = fopen(statfile, "r");
	if (fd==NULL) return -1;
	if (fgets(buffer, sizeof(buffer), fd)==NULL) {
		fclose(fd);
		return -1;
	}
	fclose(fd);
	char *token = strtok(buffer, " ");
	int i;
	for (i=0; i<3; i++) token = strtok(NULL, " ");
	p->ppid = atoi(token);
	for (i=0; i<10; i++)
		token = strtok(NULL, " ");
	p->cputime = atoi(token) * 1000 / HZ;
	token = strtok(NULL, " ");
	p->cputime += atoi(token) * 1000 / HZ;
	for (i=0; i<7; i++)
		token = strtok(NULL, " ");
	p->starttime = atoi(token) / sysconf(_SC_CLK_TCK);
	//read command line
	sprintf(exefile,"/proc/%d/cmdline", p->pid);
	fd = fopen(statfile, "r");
	if (fgets(buffer, sizeof(buffer), fd)==NULL) {
		fclose(fd);
		return -1;
	}
	fclose(fd);
	sscanf(buffer, "%s", (char*)&p->command);
	return 0;
}

static pid_t getppid_of(pid_t pid)
{
	char statfile[20];
	char buffer[1024];
	sprintf(statfile, "/proc/%d/stat", pid);
	FILE *fd = fopen(statfile, "r");
	if (fd==NULL) return -1;
	if (fgets(buffer, sizeof(buffer), fd)==NULL) {
		fclose(fd);
		return -1;
	}
	fclose(fd);
	char *token = strtok(buffer, " ");
	int i;
	for (i=0; i<3; i++) token = strtok(NULL, " ");
	return atoi(token);
}

static int is_child_of(pid_t child_pid, pid_t parent_pid)
{
	int ppid = child_pid;
	while(ppid > 1 && ppid != parent_pid) {
		ppid = getppid_of(ppid);
	}
	return ppid == parent_pid;
}

int get_next_process(struct process_iterator *it, struct process *p)
{
	if (it->dip == NULL)
	{
		//end of processes
		return -1;
	}
	if (it->filter->pid > 0 && !it->filter->include_children)
	{
		read_process_info(it->filter->pid, p);
		//p->starttime += it->boot_time;
		closedir(it->dip);
		it->dip = NULL;
		return 0;
	}
	struct dirent *dit;
	//read in from /proc and seek for process dirs
	while ((dit = readdir(it->dip)) != NULL) {
		if(strtok(dit->d_name, "0123456789") != NULL)
			continue;
		p->pid = atoi(dit->d_name);
		if (it->filter->pid > 0 && it->filter->pid != p->pid && !is_child_of(p->pid, it->filter->pid)) continue;
		read_process_info(p->pid, p);
		//p->starttime += it->boot_time;
		break;
	}
	if (dit == NULL)
	{
		//end of processes
		closedir(it->dip);
		it->dip = NULL;
		return -1;
	}
	return 0;
}

int close_process_iterator(struct process_iterator *it) {
	if (it->dip != NULL && closedir(it->dip) == -1) {
		perror("closedir");
		return 1;
	}
	it->dip = NULL;
	return 0;
}