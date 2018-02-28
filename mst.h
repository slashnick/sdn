#ifndef MST_H_
#define MST_H_

#define IGNORE_LINK 0xffffffff

void start_mst(void);
void stop_mst(void); /* TODO */
void add_mst_edge(int, uint32_t, const void *);

#endif /* MST_H_ */
