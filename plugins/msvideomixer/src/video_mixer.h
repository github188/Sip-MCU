

typedef struct _Region{
	double x,y,w;
}Region;

typedef struct _Layout{
	int nregions;
	Region *regions;
}Layout;

