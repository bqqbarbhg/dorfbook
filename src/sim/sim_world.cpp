
typedef U32 Entity_Id;

struct Sim_Entity
{
	Entity_Id id;
	Tag_Id *tags;
	U32 tag_count;
	U32 tag_capacity;
};

struct Sim_World
{
	Sim_Info *info;

	Sim_Entity *entities;
	U32 entity_count;
	U32 entity_capacity;

	Tag_Id *tag_memory;
	U32 tag_memory_used;
	U32 tag_memory_capacity;

	Entity_Id entity_counter;
};

bool sim_world_allocate(Sim_World *world)
{
	const U32 max_entities = 128;
	world->entities = M_ALLOC(Sim_Entity, max_entities);
	if (!world->entities) return false;
	world->entity_capacity = max_entities;

	const U32 max_tags = 1024;
	world->tag_memory = M_ALLOC(Tag_Id, max_tags);
	if (!world->tag_memory) return false;
	world->tag_memory_capacity = max_tags;

	return true;
}

void sim_world_create(Sim_World *world, Sim_Info *info)
{
	world->info = info;
	world->entity_count = 0;
	world->tag_memory_used = 0;
	world->entity_counter = 0;
}

Tag_Id *sim_allocate_tags(Sim_World *w, U32 count)
{
	U32 memory_pos = w->tag_memory_used;
	if (memory_pos + count > w->tag_memory_capacity) {
		return 0;
	}
	w->tag_memory_used += count;
	return w->tag_memory + memory_pos;
}

Entity_Id sim_create_entity(Sim_World *w, Tag_Id *init_tags, U32 tag_count)
{
	if (w->entity_count + 1 > w->entity_capacity) return 0;

	Tag_Id *tags = sim_allocate_tags(w, tag_count);
	if (!tags) return 0;
	memcpy(tags, init_tags, tag_count * sizeof(Tag_Id));

	Entity_Id id = ++w->entity_counter;
	U32 index = w->entity_count++;

	Sim_Entity *entity = w->entities + index;

	entity->id = id;
	entity->tags = tags;
	entity->tag_count = tag_count;
	entity->tag_capacity = tag_count;

	return id;
}

