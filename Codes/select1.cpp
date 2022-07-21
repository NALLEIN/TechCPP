asmlinkage long sys_poll(struct pollfd * ufds, unsigned int nfds, long timeout)
{
	int i, j, fdcount, err;
	struct pollfd **fds;
	struct poll_wqueues table, *wait;
	int nchunks, nleft;

	/* Do a sanity check on nfds ... */
	if (nfds > NR_OPEN)
		return -EINVAL;

	if (timeout) {
		/* Careful about overflow in the intermediate values */
		if ((unsigned long) timeout < MAX_SCHEDULE_TIMEOUT / HZ)
			timeout = (unsigned long)(timeout*HZ+999)/1000+1;
		else /* Negative or overflow */
			timeout = MAX_SCHEDULE_TIMEOUT;
	}
	// 2. 注册回调函数__pollwait
	poll_initwait(&table);
	wait = &table;
	if (!timeout)
		wait = NULL;

	err = -ENOMEM;
	fds = NULL;
	if (nfds != 0) {
		fds = (struct pollfd **)kmalloc(
			(1 + (nfds - 1) / POLLFD_PER_PAGE) * sizeof(struct pollfd *),
			GFP_KERNEL);
		if (fds == NULL)
			goto out;
	}

	nchunks = 0;
	nleft = nfds;
	while (nleft > POLLFD_PER_PAGE) { /* allocate complete PAGE_SIZE chunks */
		fds[nchunks] = (struct pollfd *)__get_free_page(GFP_KERNEL);
		if (fds[nchunks] == NULL)
			goto out_fds;
		nchunks++;
		nleft -= POLLFD_PER_PAGE;
	}
	if (nleft) { /* allocate last PAGE_SIZE chunk, only nleft elements used */
		fds[nchunks] = (struct pollfd *)__get_free_page(GFP_KERNEL);
		if (fds[nchunks] == NULL)
			goto out_fds;
	}

	err = -EFAULT;
	for (i=0; i < nchunks; i++)
		// 
		if (copy_from_user(fds[i], ufds + i*POLLFD_PER_PAGE, PAGE_SIZE))
			goto out_fds1;
	if (nleft) {
		if (copy_from_user(fds[nchunks], ufds + nchunks*POLLFD_PER_PAGE, 
				nleft * sizeof(struct pollfd)))
			goto out_fds1;
	}

	fdcount = do_poll(nfds, nchunks, nleft, fds, wait, timeout);

	/* OK, now copy the revents fields back to user space. */
	for(i=0; i < nchunks; i++)
		for (j=0; j < POLLFD_PER_PAGE; j++, ufds++)
			__put_user((fds[i] + j)->revents, &ufds->revents);
	if (nleft)
		for (j=0; j < nleft; j++, ufds++)
			__put_user((fds[nchunks] + j)->revents, &ufds->revents);

	err = fdcount;
	if (!fdcount && signal_pending(current))
		err = -EINTR;

out_fds1:
	if (nleft)
		free_page((unsigned long)(fds[nchunks]));
out_fds:
	for (i=0; i < nchunks; i++)
		free_page((unsigned long)(fds[i]));
	if (nfds != 0)
		kfree(fds);
out:
	poll_freewait(&table);
	return err;
}

static int do_poll(unsigned int nfds, unsigned int nchunks, unsigned int nleft, 
	struct pollfd *fds[], struct poll_wqueues *wait, long timeout)
{
	int count;
	poll_table* pt = &wait->pt;

	for (;;) {
		unsigned int i;

		set_current_state(TASK_INTERRUPTIBLE);
		count = 0;
		for (i=0; i < nchunks; i++)
			do_pollfd(POLLFD_PER_PAGE, fds[i], &pt, &count);
		if (nleft)
			do_pollfd(nleft, fds[nchunks], &pt, &count);
		pt = NULL;
		if (count || !timeout || signal_pending(current))
			break;
		count = wait->error;
		if (count)
			break;
		timeout = schedule_timeout(timeout);
	}
	current->state = TASK_RUNNING;
	return count;
}

void __pollwait(struct file *filp, wait_queue_head_t *wait_address, poll_table *_p)
{
	struct poll_wqueues *p = container_of(_p, struct poll_wqueues, pt);
	struct poll_table_page *table = p->table;

	if (!table || POLL_TABLE_FULL(table)) {
		struct poll_table_page *new_table;

		new_table = (struct poll_table_page *) __get_free_page(GFP_KERNEL);
		if (!new_table) {
			p->error = -ENOMEM;
			__set_current_state(TASK_RUNNING);
			return;
		}
		new_table->entry = new_table->entries;
		new_table->next = table;
		p->table = new_table;
		table = new_table;
	}

	/* 添加新节点 */
	{
		struct poll_table_entry * entry = table->entry;
		table->entry = entry+1;
	 	get_file(filp);
	 	entry->filp = filp;
		entry->wait_address = wait_address;
		init_waitqueue_entry(&entry->wait, current);
		add_wait_queue(wait_address,&entry->wait);
	}
}

static void do_pollfd(unsigned int num, struct pollfd * fdpage,
	poll_table ** pwait, int *count)
{
	int i;

	for (i = 0; i < num; i++) {
		int fd;
		unsigned int mask;
		struct pollfd *fdp;

		mask = 0;
		fdp = fdpage+i;
		fd = fdp->fd;
		if (fd >= 0) {
			struct file * file = fget(fd);
			mask = POLLNVAL;
			if (file != NULL) {
				mask = DEFAULT_POLLMASK;
				if (file->f_op && file->f_op->poll)
					mask = file->f_op->poll(file, *pwait);
				mask &= fdp->events | POLLERR | POLLHUP;
				fput(file);
			}
			if (mask) {
				*pwait = NULL;
				(*count)++;
			}
		}
		fdp->revents = mask;
	}
}