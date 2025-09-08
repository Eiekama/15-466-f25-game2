#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <random>
#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	bool game_over = false;

	Scene::Transform *player;
	glm::vec3 start_pos;
	bool jump = false;
	float jump_cooldown = 0.7;
	float jump_timer = 0;
	float jump_fn(float t);
	
	float spawn_timer = 3;

	bool random_bool();
	float random_float(float low, float high);

	//camera:
	Scene::Camera *camera = nullptr;

};
