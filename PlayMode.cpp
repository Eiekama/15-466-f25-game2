#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
// #include "GameObject.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <cmath>
#include <cstdlib>
#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <list>
#include <vector>

GLuint dinogame_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > dinogame_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("dino-game.pnct"));
	dinogame_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > dinogame_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("dino-game.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = dinogame_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = dinogame_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});



struct GameObject {
	virtual ~GameObject() {}

	Scene::Transform *transform;
	std::list< Scene::Drawable >::iterator idx;

	void destroy_self(Scene &scene) {
		scene.drawables.erase(idx);
		delete transform;
	}

	/// returns true if game object was destroyed this update
	virtual bool update(Scene &scene, float elapsed) { return false; }

	GameObject(Scene &scene, std::string const &mesh_name, glm::vec3 position) {
		Mesh const &mesh = dinogame_meshes->lookup(mesh_name);
		transform = new Scene::Transform();
		transform->position = position;
		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = dinogame_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

		idx = std::prev(scene.drawables.end());
	}
};

struct Projectile : GameObject {
	float speed;
	float lifetime = 2;

	virtual bool update(Scene &scene, float elapsed) override {
		transform->position += glm::vec3(0, speed * elapsed, 0);
		lifetime -= elapsed;
		if (lifetime < 0) {
			destroy_self(scene);
			return true;
		}
		return false;
	}

	Projectile(Scene &scene, std::string const &mesh_name, glm::vec3 position, float speed) :
		GameObject(scene, mesh_name, position), speed(speed)
	{}
};

std::list<Projectile> obstacles;
std::list<Projectile> projectiles;


float PlayMode::jump_fn(float t) {
	float h = 3;
	float g = 5;
	float x = g*t - std::sqrt(h);
	return std::max(h - x*x, 0.f);
}

bool PlayMode::random_bool() {
	return rand() % 2;
}

// from https://stackoverflow.com/questions/686353/random-float-number-generation
float PlayMode::random_float(float low, float high) {
	return low + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/(high-low)));
}



PlayMode::PlayMode() : scene(*dinogame_scene) {
	
	for (auto &transform : scene.transforms) {
		if (transform.name == "Player") {
			player = &transform;
			start_pos = transform.position;
		}
	}

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		} else if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			if (!game_over && !jump) {
				jump = true;
			}
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			if (!game_over) {
				float dz = player->position.z - start_pos.z;
				float pz;
				if (dz < 1.5) pz = 3;
				else pz = 5;
				projectiles.emplace_back(Projectile(scene, "Projectile", glm::vec3(0,player->position.y + 2,pz), 20));
			}
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		}
	}
	else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			if (game_over) {
				projectiles.clear();
				obstacles.clear();
				Mode::set_current(std::make_shared< PlayMode >());
			}
			return true;
		}

	return false;
}

void PlayMode::update(float elapsed) {

	if (!game_over)
	{
		if (jump) {
			jump_timer += elapsed;
			player->position = start_pos + glm::vec3(0, 0, jump_fn(jump_timer));
			if (jump_timer > jump_cooldown) {
				jump = false;
				jump_timer = 0;
			}
		}

		// spawn obstacles
		if (spawn_timer < 0) {
			if (random_bool())
				obstacles.emplace_back(Projectile(scene, "Obstacle", glm::vec3(0,15,1), -20));
			if (random_bool())
				obstacles.emplace_back(Projectile(scene, "Obstacle", glm::vec3(0,15,3), -20));
			if (random_bool())
				obstacles.emplace_back(Projectile(scene, "Obstacle", glm::vec3(0,17,5), -20));
			spawn_timer = random_float(1.5, 3);
		}
		else {
			spawn_timer -= elapsed;
		}

		// update obstacles and check for collision with player
		std::list< Projectile >::iterator o;
		for (o = obstacles.begin(); o != obstacles.end();) {

			glm::vec3 opos = o->transform->position;
			glm::vec3 ppos = player->position;
			if (std::abs(opos.y - ppos.y) < 2 && std::abs(opos.z - ppos.z) < 3) {
				game_over = true;
			}

			if (o->update(scene, elapsed)) o = obstacles.erase(o);
			else ++o;
		}

		// update projectiles and check for collision with obstacles
		std::list< Projectile >::iterator p;
		for (p = projectiles.begin(); p != projectiles.end();) {

			bool collided = false;
			glm::vec3 ppos = p->transform->position;
			for (o = obstacles.begin(); o != obstacles.end(); ++o) {
				if (glm::length(ppos - o->transform->position) < 2) {
					o->destroy_self(scene);
					obstacles.erase(o);
					p->destroy_self(scene);
					p = obstacles.erase(p);
					collided = true;
					break;
				}
			}

			if (!collided) {
				if (p->update(scene, elapsed)) p = projectiles.erase(p);
				else ++p;
			}
		}
	}


	//slowly rotates through [0,1):
	// wobble += elapsed / 10.0f;
	// wobble -= std::floor(wobble);

	// hip->rotation = hip_base_rotation * glm::angleAxis(
	// 	glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 1.0f, 0.0f)
	// );
	// upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
	// 	glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 0.0f, 1.0f)
	// );
	// lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
	// 	glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 0.0f, 1.0f)
	// );

	//move camera:
	// {

	// 	//combine inputs into a move:
	// 	constexpr float PlayerSpeed = 30.0f;
	// 	glm::vec2 move = glm::vec2(0.0f);
	// 	if (left.pressed && !right.pressed) move.x =-1.0f;
	// 	if (!left.pressed && right.pressed) move.x = 1.0f;
	// 	if (down.pressed && !up.pressed) move.y =-1.0f;
	// 	if (!down.pressed && up.pressed) move.y = 1.0f;

	// 	//make it so that moving diagonally doesn't go faster:
	// 	if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

	// 	glm::mat4x3 frame = camera->transform->make_parent_from_local();
	// 	glm::vec3 frame_right = frame[0];
	// 	//glm::vec3 up = frame[1];
	// 	glm::vec3 frame_forward = -frame[2];

	// 	camera->transform->position += move.x * frame_right + move.y * frame_forward;
	// }

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	// { //use DrawLines to overlay some text:
	// 	glDisable(GL_DEPTH_TEST);
	// 	float aspect = float(drawable_size.x) / float(drawable_size.y);
	// 	DrawLines lines(glm::mat4(
	// 		1.0f / aspect, 0.0f, 0.0f, 0.0f,
	// 		0.0f, 1.0f, 0.0f, 0.0f,
	// 		0.0f, 0.0f, 1.0f, 0.0f,
	// 		0.0f, 0.0f, 0.0f, 1.0f
	// 	));

	// 	constexpr float H = 0.09f;
	// 	lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
	// 		glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
	// 		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
	// 		glm::u8vec4(0x00, 0x00, 0x00, 0x00));
	// 	float ofs = 2.0f / drawable_size.y;
	// 	lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
	// 		glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
	// 		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
	// 		glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	// }
}
