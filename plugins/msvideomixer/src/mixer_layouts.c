#include "video_mixer.h"
#include "stdlib.h"

/* this file defines position of video pictures within the stitcher frame */
/* 
Coordinates are expressed in percentage of the width/height of the 
stitcher frame and point the center of the video input.
*/

static Region regions_1[]={
	{ 0.5 , 0.5 , 1 }
};

static Region regions_2[]={
	{ 0.25,	0.5,	0.5 },
	{ 0.75, 0.5,	0.5 }
};

static Region regions_3[]={
	{ 0.25,	0.25,	0.5 },
	{ 0.75, 0.25,	0.5 },
	{ 0.55, 0.75,	0.5 }
};

static Region regions_4[]={
	{ 0.25,	0.25,	0.5 },
	{ 0.75, 0.25,	0.5 },
	{ 0.25, 0.75,	0.5 },
	{ 0.75,	0.75,	0.5 }
};

#if 1

static Region regions_5[]={
	{ 0.17,	0.17,	0.33 },
	{ 0.83,	0.17,	0.33 },

	{ 0.5,	0.5,	0.33 },

	{ 0.17,	0.83,	0.33 },
	{ 0.83,	0.83,	0.33 },
};

// 5+1 ²¼¾Ö
static Region regions_6[]={
	{ 0.33,	0.33,	0.65 },
	{ 0.83,	0.17,	0.33 },
	{ 0.83,	0.5,	0.33 },

	{ 0.17,	0.83,	0.33 },
	{ 0.5,	0.83,	0.33 },

	{ 0.83,	0.83,	0.33 },
};

/*
static Region regions_6[]={
	{ 0.17,	0.17,	0.33 },
	{ 0.83,	0.17,	0.33 },

	{ 0.17,	0.5,	0.33 },
	{ 0.83,	0.5,	0.33 },

	{ 0.17,	0.83,	0.33 },
	{ 0.83,	0.83,	0.33 },
};
*/


static Region regions_7[]={
	{ 0.17,	0.17,	0.33 },
	{ 0.5,	0.17,	0.33 },
	{ 0.83,	0.17,	0.33 },

	{ 0.5,	0.5,	0.33 },

	{ 0.17,	0.83,	0.33 },
	{ 0.5,	0.83,	0.33 },
	{ 0.83,	0.83,	0.33 },
};

static Region regions_8[]={
	{ 0.17,	0.17,	0.33 },
	{ 0.5,	0.17,	0.33 },
	{ 0.83,	0.17,	0.33 },

	{ 0.17,	0.5,	0.33 },
	{ 0.83,	0.5,	0.33 },

	{ 0.17,	0.83,	0.33 },
	{ 0.5,	0.83,	0.33 },
	{ 0.83,	0.83,	0.33 },
};

static Region regions_9[]={
	{ 0.17,	0.17,	0.33 },
	{ 0.5,	0.17,	0.33 },
	{ 0.83,	0.17,	0.33 },

	{ 0.17,	0.5,	0.33 },
	{ 0.5,	0.5,	0.33 },
	{ 0.83,	0.5,	0.33 },

	{ 0.17,	0.83,	0.33 },
	{ 0.5,	0.83,	0.33 },
	{ 0.83,	0.83,	0.33 },
};


#endif

Layout video_mixer_default_layout[]={
	{	1,	regions_1	},
	{	2,	regions_2	},
	{	3,	regions_3	},
	{	4,	regions_4	},
	{	5,	regions_5	},
	{	6,	regions_6	},
	{	7,	regions_7	},
	{	8,	regions_8	},
	{	9,	regions_9	},
	{	0,	NULL		}
};
