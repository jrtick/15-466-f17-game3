#include "load_save_png.hpp"
#include "GL.hpp"
#include "Meshes.hpp"
#include "Scene.hpp"
#include "read_chunk.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <fstream>

#include <stdlib.h>
#include <time.h>
#include "player.hpp"

#define RANDOM() (((float)rand())/RAND_MAX)

static GLuint compile_shader(GLenum type, std::string const &source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);

#define NUM_PLAYERS 5 //at least 2 for you and opponent is necessary
#define NUM_PLAYERS_STR "5" //it's a preprocess thing sorry

int main(int argc, char **argv) {
	srand(time(0));
	//Configuration:
	struct {
		std::string title = "Game3: Ultimate Blob Tag";
		glm::uvec2 size = glm::uvec2(1280, 960);
	} config;

	//------------  initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		config.title.c_str(),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.size.x, config.size.y,
		SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
	);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	#ifdef _WIN32
	//On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
	#endif

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	//Hide mouse cursor (note: showing can be useful for debugging):
	//SDL_ShowCursor(SDL_DISABLE);

	//------------ opengl objects / game assets ------------
	//shader program:
	GLuint program = 0;
	GLuint program_Position = 0;
	GLuint program_Normal = 0;
	GLuint program_Color = 0;
	GLuint program_SkinColor = 0;
	GLuint program_ShirtColor = 0;
	GLuint program_PlayerIdx = 0;
	GLuint program_mvp = 0;
	GLuint program_itmv = 0;
	GLuint program_to_light = 0;
	{ //compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
			"#version 330\n"
			"uniform mat4 mvp;\n"
			"uniform mat3 itmv;\n"
			"uniform vec3 ShirtColor[" NUM_PLAYERS_STR "];\n"
			"uniform vec3 SkinColor[" NUM_PLAYERS_STR "];\n"	
			"uniform int PlayerIdx;\n"
			"in vec4 Position;\n"
			"in vec3 Normal;\n"
			"in vec3 Color;\n"
			"out vec3 normal;\n"
			"out vec3 color;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"	normal = itmv * Normal;\n"
			"	if(PlayerIdx<0) color = Color;\n"
			"	else if(Color.x < 0.5) color = ShirtColor[PlayerIdx];\n"
			"	else                   color = SkinColor[PlayerIdx];\n"
			"}\n"
		);

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
			"#version 330\n"
			"uniform vec3 to_light;\n"
			"in vec3 normal;\n"
			"in vec3 color;\n"
			"in int idx;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	float light = max(0.0, dot(normalize(normal), to_light));\n"
			"	fragColor = vec4(light*color, 1.0);\n"
			"}\n"
		);

		program = link_program(fragment_shader, vertex_shader);

		//look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1U) throw std::runtime_error("no attribute named Position");
		program_Normal = glGetAttribLocation(program, "Normal");
		if (program_Normal == -1U) throw std::runtime_error("no attribute named Normal");

		program_Color = glGetAttribLocation(program, "Color");
		if (program_Color == -1U) throw std::runtime_error("no attribute named Color");


		//look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1U) throw std::runtime_error("no uniform named mvp");
		program_itmv = glGetUniformLocation(program, "itmv");
		if (program_itmv == -1U) throw std::runtime_error("no uniform named itmv");

		program_to_light = glGetUniformLocation(program, "to_light");
		if (program_to_light == -1U) throw std::runtime_error("no uniform named to_light");

		program_SkinColor = glGetUniformLocation(program, "SkinColor");
		if (program_SkinColor == -1U) throw std::runtime_error("no uniform named SkinColor");
		program_ShirtColor = glGetUniformLocation(program, "ShirtColor");
		if (program_ShirtColor == -1U) throw std::runtime_error("no uniform named ShirtColor");

		program_PlayerIdx = glGetUniformLocation(program,"PlayerIdx");
		if(program_PlayerIdx == -1U) throw std::runtime_error("no uniform named PlayerIdx");
	}

	//------------ meshes ------------

	Meshes meshes;

	{ //add meshes to database:
		Meshes::Attributes attributes;
		attributes.Position = program_Position;
		attributes.Normal = program_Normal;
		attributes.Color = program_Color;

		meshes.load("meshes.blob", attributes);
	}
	
	//------------ scene ------------

	Scene scene;
	scene.program_PlayerIdx=program_PlayerIdx;
	//set up camera parameters based on window:
	scene.camera.fovy = glm::radians(60.0f);
	scene.camera.aspect = float(config.size.x) / float(config.size.y);
	scene.camera.near = 0.01f;
	//(transform will be handled in the update function below)

	//add some objects from the mesh library:
	auto add_object = [&](std::string const &name, glm::vec3 const &position, glm::quat const &rotation=glm::quat(0,0,0,0), glm::vec3 const &scale=glm::vec3(1,1,1),int playerIdx=-1) -> Scene::Object* {
		Mesh const &mesh = meshes.get(name);
		scene.objects.emplace_back();
		Scene::Object& object = scene.objects.back();
		object.transform.position = position;
		object.transform.rotation = rotation;
		object.transform.scale = scale;
		object.vao = mesh.vao;
		object.start = mesh.start;
		object.count = mesh.count;
		object.program = program;
		object.program_mvp = program_mvp;
		object.program_itmv = program_itmv;
		object.name = name;
		object.playerIdx = playerIdx; //link mesh with player for coloring purposes
		return &object;
	};

	struct {
		glm::vec3 shirtColors[NUM_PLAYERS]; //changes anytime someone's tagged
		glm::vec3 skinColors[NUM_PLAYERS]; //won't change after init
	} playerColors;

	//setup players
	Player players[NUM_PLAYERS];
	for(int i=0;i<NUM_PLAYERS;i++){
		glm::vec3 skinColor = glm::vec3(RANDOM(),RANDOM(),RANDOM());
		glm::vec3 shirtColor = glm::vec3(RANDOM(),RANDOM(),RANDOM());
		glm::vec2 pos = glm::vec2(80*RANDOM()-40,80*RANDOM()-40);
		players[i] = Player(pos,shirtColor,skinColor);
	}
	players[1].tagger = true; //set the first tagger
	players[1].max_speed *= 1.5;

	{ //read objects to add from "scene.blob". Will be ground + one player
		std::ifstream file("scene.blob", std::ios::binary);

		std::vector< char > strings;
		//read strings chunk:
		read_chunk(file, "str0", &strings);

		{ //read scene chunk, add meshes to scene:
			struct SceneEntry {
				uint32_t name_begin, name_end;
				glm::vec3 position;
				glm::quat rotation;
				glm::vec3 scale;
			};
			static_assert(sizeof(SceneEntry) == 48, "Scene entry should be packed");

			std::vector< SceneEntry > data;
			read_chunk(file, "scn0", &data);

			for (auto const &entry : data) {
				if (!(entry.name_begin <= entry.name_end && entry.name_end <= strings.size())) {
					throw std::runtime_error("index entry has out-of-range name begin/end");
				}
				std::string name(&strings[0] + entry.name_begin, &strings[0] + entry.name_end);
				if(name==std::string("left_hand")){
					for(int i=0;i<NUM_PLAYERS;i++)players[i].left_hand.obj = add_object(name,entry.position,entry.rotation,entry.scale,i);
				}else if(name==std::string("right_hand")){
					for(int i=0;i<NUM_PLAYERS;i++)players[i].right_hand.obj = add_object(name,entry.position,entry.rotation,entry.scale,i);
				}else if(name==std::string("head")){
					for(int i=0;i<NUM_PLAYERS;i++)players[i].head.obj = add_object(name,entry.position,entry.rotation,entry.scale,i);
				}else if(name==std::string("body")){
					for(int i=0;i<NUM_PLAYERS;i++)players[i].body.obj = add_object(name,entry.position,entry.rotation,entry.scale,i);
				}else add_object(name, entry.position, entry.rotation, entry.scale); //ground obj
			}
		}
	}
	for(int i=0;i<NUM_PLAYERS;i++)players[i].rectify();
	
	glm::vec2 mouse = glm::vec2(0.0f, 0.0f); //mouse position in [-1,1]x[-1,1] coordinates

	struct {
		float radius = 16.0f;
		float elevation = 0.0f;
		float azimuth = 0.0f;
		glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
	} camera;
		
	//------------ game loop ------------

	bool should_quit = false;
	while (true) {
		glm::vec2 lookdir = glm::normalize(glm::vec2(players[0].pos.x-scene.camera.transform.position.x,players[0].pos.y-scene.camera.transform.position.y));
		//handle events
		const unsigned char* state = SDL_GetKeyboardState(NULL);
		glm::vec2* accel = &(players[0].accel);
		*accel = glm::vec2();
		if(state[SDL_SCANCODE_W]) *accel += lookdir;
		if(state[SDL_SCANCODE_S]) *accel -= lookdir;
		if(state[SDL_SCANCODE_A]) *accel += glm::vec2(-lookdir.y,lookdir.x);
		if(state[SDL_SCANCODE_D]) *accel -= glm::vec2(-lookdir.y,lookdir.x);
		float len = glm::length(*accel);
		if(len>0.1) *accel /= len;
		else *accel = glm::vec2();
		players[0].space = state[SDL_SCANCODE_SPACE];

		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
			if(evt.type == SDL_KEYDOWN){
				switch(evt.key.keysym.sym){
				case SDLK_ESCAPE:
				case SDLK_q:
					should_quit = true;
					break;
				case SDLK_TAB: //zoom out
					camera.radius ++;
					break;
				case SDLK_LSHIFT: //zoom in
					camera.radius --;
					break;
				case SDLK_p:
					printf("%.2f\n",players[0].arm_rot);
				default:
					break;
				}
			}
			if (evt.type == SDL_MOUSEMOTION) {
				glm::vec2 old_mouse = mouse;
				mouse.x = (evt.motion.x + 0.5f) / float(config.size.x) * 2.0f - 1.0f;
				mouse.y = (evt.motion.y + 0.5f) / float(config.size.y) *-2.0f + 1.0f;
				if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
					camera.elevation += -2.0f * (mouse.y - old_mouse.y);
					camera.azimuth += -2.0f * (mouse.x - old_mouse.x);
				}
			} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
			} else if (evt.type == SDL_QUIT) {
				should_quit = true;
				break;
			}
		}
		if (should_quit) break;


		//update timers
		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
		float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
		previous_time = current_time;
		static float fulltime = 0;
		fulltime += elapsed;

		{ //update game state
			//move players forward
			for(int i=0;i<NUM_PLAYERS;i++){
				if(!!i) players[i].moveAI(players,NUM_PLAYERS);
				players[i].update(elapsed);
			}

			//check collision detection
			bool colliding[NUM_PLAYERS] = {false};
			for(int i=0;i<NUM_PLAYERS;i++){
				Player* curPlayer = &players[i];
				//if(colliding[i]) continue;
				for(int j=i+1;j<NUM_PLAYERS;j++){
					//if(colliding[j]) continue;

					Player::Collision col;
					if((col=curPlayer->collides(&players[j]))!=Player::Collision::None){
						switch(col){
						case Player::Collision::LeftBody:
						case Player::Collision::RightBody:
							//if(!curPlayer->colliding && !players[j].colliding){
								if(curPlayer->tagged(&players[j]))
									printf("%d tagged %d!\n",i,j);
							//}
							break;
						case Player::Collision::BodyLeft:
						case Player::Collision::BodyRight:
							//if(!curPlayer->colliding && !players[j].colliding){
								if(players[j].tagged(curPlayer))
									printf("%d tagged %d!\n",j,i);
							//}
							break;
						default:
							break;
						}
						//stop both players
						if(!curPlayer->colliding && !players[j].colliding){
							curPlayer->vel = glm::vec2();
							players[j].vel = glm::vec2();
						}
						curPlayer->colliding=players[j].colliding = true; //were colliding. don't count twice
						colliding[i]=colliding[j]=true;
					}
				}
			}
			for(int i=0;i<NUM_PLAYERS;i++) players[i].colliding=colliding[i];

			//update colors if necessary
			if(Player::colorsDirty){
				bool gameover = true;
				glm::vec3 firstColor=players[0].shirtColor;
				for(int i=0;i<NUM_PLAYERS;i++){
					playerColors.shirtColors[i] = players[i].shirtColor;
					if(glm::length(players[i].shirtColor-firstColor)>0.1) gameover = false;
					playerColors.skinColors[i] = players[i].skinColor;
				}

				if(gameover){
					printf("Game over!\n");
					for(int i=0;i<NUM_PLAYERS;i++) printf("Player %d's score: %.2f\n",i,players[i].score);
					should_quit = true;
				}
				Player::colorsDirty = false;
			}

			camera.target = glm::vec3(players[0].pos,0);
			//camera
			scene.camera.transform.position = camera.radius * glm::vec3(
				std::cos(camera.elevation) * std::cos(camera.azimuth),
				std::cos(camera.elevation) * std::sin(camera.azimuth),
				std::sin(camera.elevation)) + camera.target;

			glm::vec3 out = -glm::normalize(camera.target - scene.camera.transform.position);
			glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
			up = glm::normalize(up - glm::dot(up, out) * out);
			glm::vec3 right = glm::cross(up, out);
			
			scene.camera.transform.rotation = glm::quat_cast(
				glm::mat3(right, up, out)
			);
			scene.camera.transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
		}

		//draw output
		glClearColor(0.5, 0.5, 0.5, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		{ //draw game state
			glUseProgram(program);
			glUniform3fv(program_to_light, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 1.0f, 10.0f))));
			glUniform3fv(program_ShirtColor, NUM_PLAYERS, (GLfloat*)playerColors.shirtColors);
			glUniform3fv(program_SkinColor, NUM_PLAYERS, (GLfloat*)playerColors.skinColors);
			scene.render();
		}


		SDL_GL_SwapWindow(window);
	}


	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}



static GLuint compile_shader(GLenum type, std::string const &source) {
	GLuint shader = glCreateShader(type);
	GLchar const *str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
