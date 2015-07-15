#include <stdio.h>
#include <stdlib.h>

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
	bool alive;
};

enum Post_Type
{
	Post_Death,
	Post_Activity,
};

struct Post
{
	U32 by_id;
	Post_Type type;
	U64 data;
};

struct World
{
	Dwarf dwarves[64];
	Post posts[128];
	U32 post_index;

	Random_Series random_series;
};

void world_post(World *world, U32 id, Post_Type type, U64 data)
{
	world->post_index = (world->post_index + 1) % Count(world->posts);
	Post *post = &world->posts[world->post_index];
	post->by_id = id;
	post->type = type;
	post->data = data;
}

void dwarf_do_activity(World *world, Dwarf *dwarf, Activity activity)
{
	Random_Series *rs = &world->random_series;

	dwarf->activity = activity;
	if (activity != Activity_Idle && next_one_in(rs, 100)) {
		world_post(world, dwarf->id, Post_Activity, activity);
	}
}

const char *dwarf_status(Dwarf *dwarf)
{
	if (!dwarf->alive)
		return "Dead";
	return activity_infos[dwarf->activity].description;
}

void world_tick(World *world)
{
	Random_Series *rs = &world->random_series;

	for (U32 i = 0; i < Count(world->dwarves); i++) {
		Dwarf *dwarf = &world->dwarves[i];
		if (!dwarf->id || !dwarf->alive)
			continue;

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

		if (next_one_in(rs, 100000)) {
			world_post(world, dwarf->id, Post_Death, 0);
			dwarf->alive = false;
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
				dwarf->id, dwarf->name, dwarf_status(dwarf));
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
		
		switch (post->type) {

		case Post_Activity:
			ptr += sprintf(ptr, "I will go %s", activity_infos[post->data].description);
			break;

		case Post_Death:
			ptr += sprintf(ptr, "Died suddenly");
			break;

		}
		ptr += sprintf(ptr, "</li>\n");
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
	ptr += sprintf(ptr, "<h2>%s</h2>", dwarf_status(dwarf)); 
	ptr += sprintf(ptr, "<h3>Hunger: %d, sleep: %d</h3>", dwarf->hunger, dwarf->sleep); 
	ptr += sprintf(ptr, "</body></html>"); 

	return 200;
}

