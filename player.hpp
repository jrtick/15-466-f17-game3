#include <glm/glm.hpp>
#include "Scene.hpp"

class Sphere{
public:
	glm::vec3 center;
	float radius;
	Scene::Object* obj;
	Sphere(){}
	Sphere(glm::vec3 center,float radius=1){
		this->center = center;
		this->radius = radius;
	}
};
class Cylinder{
public:
	glm::vec3 center; //center of bottom circle
	glm::vec3 axis; //points to center of top circle; normalized?
	float length,radius;
	Scene::Object* obj;
	Cylinder(){}
	Cylinder(glm::vec3 center, glm::vec3 axis, float length=1,float radius=1){
		this->center = center;
		this->axis = axis;
		this->length = length;
		this->radius = radius;
	}
};

float bobbingFunction(float duration); //returns height to move character to

class Player{
public:
	static bool colorsDirty; //whether someone's shirt color has changed
	static float bodyLength,bodyRadius,armRadius,headRadius;
	static glm::vec3 left_arm_offset,right_arm_offset,head_offset;
	static glm::vec2 minPos,maxPos;

	enum class Collision {LeftLeft,RightRight,LeftRight,RightLeft, //hands
		LeftBody,RightBody,BodyLeft,BodyRight,
		BodyBody,None};
	glm::vec2 pos = glm::vec2(0,0);//constrained to z=0 plane
	glm::vec2 vel = glm::vec2(0,0);
	glm::vec2 accel = glm::vec2(0,0);
	
	int score = 0;
	float accel_mag = 50;
	float max_speed = 50;
	float walking_dur=0;//keep track to simulate bobbing
	float body_rot=0;
	float arm_rot=0; //0 is hands by side, 90 is hands in front
	bool space = false; //holding down spacebar
	bool colliding = false; //will preven collisions counting multiple times
	bool tagger = false; // will determine if player can tag others
	Sphere left_hand,right_hand,head;
	Cylinder body;
	glm::vec3 skinColor,shirtColor;
	Player(){};
	Player(glm::vec2 pos,glm::vec3 shirtColor,glm::vec3 skinColor){
		this->shirtColor = shirtColor;
		this->skinColor = skinColor;
		this->pos = pos;
	
		//set positions of colliders
		glm::vec3 pos3 = glm::vec3(pos.x,pos.y,0);
		body = Cylinder(pos3, glm::vec3(0,0,1),bodyLength,bodyRadius);
		head = Sphere(pos3+head_offset,headRadius);
		right_hand = Sphere(pos3+left_arm_offset,armRadius);
		left_hand = Sphere(pos3+right_arm_offset,armRadius);
	}
	void rectify(){ //TO BE CALLED AFTER POINTERS IN MAIN ARE SET
		//setup body hierarchy
		Scene::Transform* par = &(body.obj->transform);
		left_hand.obj->transform.set_parent(par);
		right_hand.obj->transform.set_parent(par);
		head.obj->transform.set_parent(par);

		//make transforms match colliders
		body.obj->transform.position = body.center;

	}
	bool tagged(Player* player){
		if(tagger && glm::length(player->shirtColor-shirtColor)>0.01f){
			score += 10;
			player->shirtColor = shirtColor;
			player->tagger = true;
			colorsDirty = true;
			return true;
		}else return false;
	}
	Collision collides(Player* other);
	void moveAI(Player* players,int numPlayers){
		float maxdist = glm::length(minPos-maxPos);
		if(tagger){ //go towards closest nontagger
			glm::vec2 target = pos;
			float targetDist = maxdist;
			for(int i=0;i<numPlayers;i++){
				if(!players[i].tagger){
					float dist = glm::length(players[i].pos-pos);
					if(dist <= targetDist){
						targetDist = dist;
						target = players[i].pos;
					}
				}
			}
			accel = glm::normalize(target-pos);
			space = targetDist < 10;
		}else{ //go in direction opposite average weighted position of enemies
			glm::vec2 weightedPos = glm::vec2();
			float sumweights = 0;
			for(int i=0;i<numPlayers;i++){
				if(players[i].tagger){
					float weight = 1/pow(glm::length(players[i].pos-pos),2);
					weightedPos += players[i].pos*weight;
					sumweights += weight;
				}
			}
			for(int i=0;i<4;i++){ //pretend corners are taggers so players don't run off board
				glm::vec2 corner1,corner2;
				switch(i){
				case 0:
					corner1 = minPos;
					corner2 = glm::vec2(minPos.x,maxPos.y);
					break;
				case 1:
					corner1 = glm::vec2(minPos.x,maxPos.y);
					corner2 = maxPos;
					break;
				case 2:
					corner1 = maxPos;
					corner2 = glm::vec2(maxPos.x,minPos.y);
					break;
				default:
					corner1 = glm::vec2(maxPos.x,minPos.y);
					corner2 = minPos;
				}
				for(int detail=0;detail<16;detail++){
					float val = detail/float(16);
					glm::vec2 curPt = corner1*(1.f-val)+corner2*val;
					float weight = 1/pow(glm::length(curPt-pos),2);
					weightedPos += curPt*weight;
					sumweights += weight;
				}
			}
			weightedPos /= sumweights;
			accel = -glm::normalize(weightedPos-pos);
		}
	}
	void update(float elapsed){
		if(!tagger) score += int(elapsed*100);
		walking_dur += elapsed;
		
		//calculate physics
		glm::vec2 disp = vel*elapsed;
		vel += (accel_mag*accel-vel)*elapsed;
		float speed = glm::length(vel);
		if(speed>max_speed){
			vel *= max_speed/speed;
			speed = max_speed;
		}
		

		//update position
		float newHeight = bobbingFunction(0.1*speed*walking_dur);
		glm::vec3 disp3D = glm::vec3(disp.x,disp.y,newHeight - body.center.z);
		pos += disp; //2d board pos
		body.center += disp3D;
		head.center += disp3D;
		left_hand.center += disp3D;
		right_hand.center += disp3D;
		body.obj->transform.position += disp3D;
		
		//update body rotation
		body_rot = -90+glm::degrees(atan2(vel.y,vel.x));

		//update arm rotation
		if(space){
			arm_rot += 80*10*elapsed;
			if(arm_rot>80) arm_rot = 80;
		}else{
			arm_rot -= 80*10*elapsed;
			if(arm_rot<0) arm_rot = 0;
		}
#define ROTATE(x) (glm::mat4_cast(glm::angleAxis(glm::radians(x),glm::vec3(0,0,1))))
		glm::vec4 tempRight = ROTATE(-arm_rot+body_rot)*glm::vec4(right_arm_offset,1);
		glm::vec4 tempLeft = ROTATE(arm_rot+body_rot)*glm::vec4(left_arm_offset,1);
		right_hand.obj->transform.position = glm::vec3(tempRight.x,tempRight.y,tempRight.z)/tempRight.w;
		left_hand.obj->transform.position = glm::vec3(tempLeft.x,tempLeft.y,tempLeft.z)/tempLeft.w;
	
		left_hand.center = left_hand.obj->transform.get_world_position();
		right_hand.center = left_hand.obj->transform.get_world_position();

		head.obj->transform.rotation = glm::angleAxis(glm::radians(body_rot),glm::vec3(0,0,1));
	}
};
