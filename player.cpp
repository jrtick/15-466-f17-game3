#include "player.hpp"

float bobbingFunction(float duration){
	return 0.2*(0.5+0.5*sin(duration));
}
bool Player::colorsDirty = true;
//hardcoded offsets from blender file :(
float Player::bodyLength = 1.66;
float Player::bodyRadius = 1;
float Player::armRadius = 0.33;
float Player::headRadius = 1;
glm::vec3 Player::left_arm_offset = glm::vec3(1.8,0,0.6);	
glm::vec3 Player::right_arm_offset = glm::vec3(-1.8,0,0.6);
glm::vec3 Player::head_offset = glm::vec3(0,2.66,0);
glm::vec2 Player::minPos = glm::vec2(-100.f,-100.f);
glm::vec2 Player::maxPos = glm::vec2(100.f,100.f);

/*********** COLLISION FUNCTIONS *****************/
bool collision(Sphere* sphere1,Sphere* sphere2){
	float dist = glm::length(sphere1->center-sphere2->center);
	return dist <= (sphere1->radius+sphere2->radius);
}

//ASSUMES Z is height and cylinders are aligned to z axis
bool collision(Cylinder* cylinder1,Cylinder* cylinder2){
	float dist = glm::length(cylinder1->center-cylinder2->center);
	bool mayCollide = (dist <= (cylinder1->radius+cylinder2->radius));
	if(mayCollide){
		float z1low = cylinder1->center.z,
			z1high = z1low+cylinder1->length,
			z2low = cylinder2->center.z,
			z2high = z2low+cylinder2->length;
		return !((z2high<z1low) || (z2low>z1high));
	}else return false;
}

glm::vec3 myDot(glm::vec3 a, glm::vec3 b){
	return b*(a.x*b.x+a.y*b.y+a.z*b.z);
}
bool collision(Sphere* sphere,Cylinder* cylinder){
	//get closest val to sphere on cylinder's axis
	glm::vec3 c = cylinder->center;
	glm::vec3 s = sphere->center;
	glm::vec3 closestOnAxis = c+myDot(s-c,cylinder->axis);

	//clamp to be in cylinder
	if(closestOnAxis.z<c.z) closestOnAxis.z = c.z;
	else if(closestOnAxis.z>c.z+cylinder->length) closestOnAxis.z=c.z+cylinder->length;

	//get point on cylinder's outside at this level
	glm::vec3 dir = glm::normalize(glm::vec3(s.x-c.x,s.y-c.y,0));
	glm::vec3 closestOnCyl = closestOnAxis+dir*cylinder->radius;

	//now see if closest point to sphere is in sphere
	return glm::length(s-closestOnCyl)<=sphere->radius;
}
inline bool collision(Cylinder* cylinder,Sphere* sphere){return collision(sphere,cylinder);}

/*********** END COLLISION FUNCTIONS *****************/

Player::Collision Player::collides(Player* other){
	Player::Collision res = Player::Collision::None;

	if(collision(&body,&other->left_hand)) res=Player::Collision::BodyLeft;
	if(collision(&body,&other->right_hand)) res=Player::Collision::BodyRight;
	if(collision(&right_hand,&other->body)) res=Player::Collision::RightBody;
	if(collision(&left_hand,&other->body)) res=Player::Collision::LeftBody;

	if(collision(&body,&other->body)) res= Player::Collision::BodyBody;

	if(collision(&right_hand,&other->right_hand)) res=Player::Collision::RightRight;
	if(collision(&right_hand,&other->left_hand)) res=Player::Collision::RightLeft;
	if(collision(&left_hand,&other->right_hand)) res=Player::Collision::LeftRight;
	if(collision(&left_hand,&other->left_hand)) res=Player::Collision::LeftLeft;

	return res;
}
