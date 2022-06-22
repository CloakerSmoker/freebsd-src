void		pxe_enable(void *pxeinfo);
extern void	(*pxe_call)(int func, void *ptr);
void	pxenv_call(int func, void *ptr);
void	bangpxe_call(int func, void *ptr);

int	pxe_init(void);
int	pxe_print(int verbose);
void	pxe_cleanup(void);