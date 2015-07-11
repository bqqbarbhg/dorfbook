#include <stdio.h>
#include <stdint.h>

typedef int32_t I32;
typedef uint32_t U32;
typedef int64_t I64;
#define Count(array) (sizeof(array)/sizeof(*(array)))

enum Activity
{
	Activity_Idle,
	Activity_Eat,
	Activity_Sleep,
};

struct Activity_Info
{
	const char *description;
} activity_infos[] = {
	{ "Idling" },
	{ "Eating" },
	{ "Sleeping" },
};

struct Dwarf
{
	U32 id;
	const char *name;
	I32 hunger;
	I32 sleep;
	Activity activity;
};

struct Post
{
	U32 by_id;
	Activity activity;
};

struct World
{
	Dwarf dwarves[64];
	Post posts[128];
	U32 post_index;
};

void world_post(World *world, U32 id, Activity activity)
{
	world->post_index = (world->post_index + 1) % Count(world->posts);
	Post *post = &world->posts[world->post_index];
	post->by_id = id;
	post->activity = activity;
}

void dwarf_do_activity(World *world, Dwarf *dwarf, Activity activity)
{
	dwarf->activity = activity;
	world_post(world, dwarf->id, activity);
}

void world_tick(World *world)
{
	for (U32 i = 0; i < Count(world->dwarves); i++) {
		Dwarf *dwarf = &world->dwarves[i];
		dwarf->hunger++;
		dwarf->sleep++;

		switch (dwarf->activity) {

		case Activity_Idle:
			if (dwarf->sleep > 50) {
				dwarf_do_activity(world, dwarf, Activity_Sleep);
			} else if (dwarf->hunger > 50) {
				dwarf_do_activity(world, dwarf, Activity_Eat);
			}
			break;

		case Activity_Eat:
			dwarf->hunger -= 3;
			if (dwarf->hunger < 5) {
				dwarf_do_activity(world, dwarf, Activity_Idle);
			}
			break;

		case Activity_Sleep:
			dwarf->sleep -= 3;
			if (dwarf->sleep < 5) {
				dwarf_do_activity(world, dwarf, Activity_Idle);
			}
			break;

		}
	}
}

int render_dwarves(World *world, char *buffer)
{
	char *ptr = buffer;
	ptr += sprintf(ptr, "<html><head><title>Dwarves</title></head>");
	ptr += sprintf(ptr, "<body><ul>\n");
	for (U32 i = 0; i < Count(world->dwarves); i++) {
		Dwarf *dwarf = &world->dwarves[i];
		if (dwarf->id == 0)
			continue;

		ptr += sprintf(ptr, "<li><a href=\"/entities/%d\">%s</a> (%s)</li>\n",
				dwarf->id, dwarf->name, activity_infos[dwarf->activity].description);
	}
	ptr += sprintf(ptr, "</ul></body></html>\n");

	return 200;
}

int render_feed(World *world, char *buffer)
{
	char *ptr = buffer;
	ptr += sprintf(ptr, "<html><head><title>Activity feed</title></head>");
	ptr += sprintf(ptr, "<body><ul>\n");
	for (U32 i = 0; i < Count(world->posts); i++) {
		Post *post = &world->posts[i];
		if (post->by_id == 0)
			continue;

		Dwarf *dwarf = 0;
		for (U32 j = 0; j < Count(world->dwarves); j++) {
			if (world->dwarves[j].id == post->by_id)
				dwarf = &world->dwarves[j];
		}
		if (!dwarf)
			continue;

		ptr += sprintf(ptr, "<li><a href=\"/entities/%d\">%s</a>:", dwarf->id, dwarf->name);
		ptr += sprintf(ptr, "I will go %s</li>\n", activity_infos[post->activity].description);
	}
	ptr += sprintf(ptr, "</ul></body></html>\n");

	return 200;
}

int render_entity(World *world, U32 id, char *buffer)
{
	char *ptr = buffer;
	Dwarf *dwarf = 0;
	for (U32 i = 0; i < Count(world->dwarves); i++) {
		if (world->dwarves[i].id == id) {
			dwarf = &world->dwarves[i];
			break;
		}
	}

	if (!dwarf) {
		sprintf(buffer, "Entity not found with ID #%u", id);
		return 404;
	}

	ptr += sprintf(ptr, "<html><head><title>%s</title></head>", dwarf->name);
	ptr += sprintf(ptr, "<body><h1>%s</h1>", dwarf->name); 
	ptr += sprintf(ptr, "<h2>%s</h2>", activity_infos[dwarf->activity].description); 
	ptr += sprintf(ptr, "<h3>Hunger: %d, sleep: %d</h3>", dwarf->hunger, dwarf->sleep); 
	ptr += sprintf(ptr, "</body></html>"); 

	return 200;
}

