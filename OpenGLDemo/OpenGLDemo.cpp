// OpenGLDemo.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
using namespace std;

// GLEW    
#define GLEW_STATIC    
#include <GL/glew.h>    

// GLFW    
#include <GLFW/glfw3.h>

#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <OpenMesh/Core/Mesh/Handles.hh>

#include "shader.h"

typedef OpenMesh::TriMesh_ArrayKernelT<> MyMesh;

MyMesh mesh;
const string file = "sphere.off";

GLuint VAO,VBO,lightVAO;

GLuint depthMapFBO;

const GLuint SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

GLuint depthMap;

vector<float> Positions;


// Window dimensions    
int WIDTH = 800, HEIGHT = 600;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
GLfloat yaw = -90.0f;	// Yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right (due to how Eular angles work) so we initially rotate a bit to the left.
GLfloat pitch = 0.0f;
GLfloat lastX = WIDTH / 2.0;
GLfloat lastY = HEIGHT / 2.0;
GLfloat fov = 45.0f;
bool keys[1024];
GLfloat modelRotateX = 0.0f;
GLfloat modelPotateY = 0.0f;

// Deltatime
GLfloat deltaTime = 0.0f;	// Time between current frame and last frame
GLfloat lastFrame = 0.0f;  	// Time of last frame

// Function prototypes
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void do_movement();

void RenderScene(Shader &shader);

//Lightpos
glm::vec3 lightPos(1.2f, 1.0f, 2.0f);


void readfile(string file) {
	// 请求顶点法线 vertex normals
	mesh.request_vertex_normals();
	//如果不存在顶点法线，则报错
	if (!mesh.has_vertex_normals())
	{
		cout << "错误：标准定点属性 “法线”不存在" << endl;
		return;
	}
	// 如果有顶点发现则读取文件
	OpenMesh::IO::Options opt;
	if (!OpenMesh::IO::read_mesh(mesh, file, opt))
	{
		cout << "无法读取文件:" << file << endl;
		return;
	}
	else cout << "成功读取文件:" << file << endl;
	cout << endl; // 为了ui显示好看一些
	//如果不存在顶点法线，则计算出
	if (!opt.check(OpenMesh::IO::Options::VertexNormal))
	{
		// 通过面法线计算顶点法线
		mesh.request_face_normals();
		// mesh计算出顶点法线
		mesh.update_normals();
		// 释放面法线
		mesh.release_face_normals();
	}
}

void Init()
{
	glShadeModel(GL_SMOOTH);
	glEnable(GL_DEPTH_TEST);



	readfile(file);

	for (MyMesh::FaceIter f_it = mesh.faces_begin(); f_it != mesh.faces_end(); ++f_it) {
		for (MyMesh::FaceVertexIter fv_it = mesh.fv_iter(*f_it); fv_it.is_valid(); ++fv_it) {

			Positions.push_back(mesh.point(*fv_it).data()[0]);
			Positions.push_back(mesh.point(*fv_it).data()[1]);
			Positions.push_back(mesh.point(*fv_it).data()[2]);

			Positions.push_back(mesh.normal(*fv_it).data()[0]);
			Positions.push_back(mesh.normal(*fv_it).data()[1]);
			Positions.push_back(mesh.normal(*fv_it).data()[2]);
		}
	}
	//FBO
	glGenFramebuffers(1, &depthMapFBO);
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	//VAO
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	// load data into vertex buffers
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	// A great thing about structs is that their memory layout is sequential for all its items.
	// The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
	// again translates to 3/2 floats which translates to a byte array.
	glBufferData(GL_ARRAY_BUFFER, sizeof(Positions[0])*Positions.size(), &Positions[0], GL_STATIC_DRAW);
	// set the vertex attribute pointers
	// vertex Positions
	//glVertexPointer(3,GL_FLOAT,0,0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (GLvoid *)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const GLvoid *)(3*sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	//LIGHT
	glGenVertexArrays(1, &lightVAO);
	glBindVertexArray(lightVAO);
	// 只需要绑定VBO不用再次设置VBO的数据，因为容器(物体)的VBO数据中已经包含了正确的立方体顶点数据
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	// 设置灯立方体的顶点属性指针(仅设置灯的顶点数据)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
}

// The MAIN function, from here we start the application and run the game loop    
int main()
{
	std::cout << "Starting GLFW context, OpenGL 3.3" << std::endl;
	// Init GLFW    
	glfwInit();
	// Set all the required options for GLFW    
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);




	// Create a GLFWwindow object that we can use for GLFW's functions    
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "LearnOpenGL", nullptr, nullptr);
	if (window == nullptr)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Set the required callback functions
	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	// Set this to true so GLEW knows to use a modern approach to retrieving function pointers and extensions    
	glewExperimental = GL_TRUE;
	// Initialize GLEW to setup the OpenGL Function pointers    
	if (glewInit() != GLEW_OK)
	{
		std::cout << "Failed to initialize GLEW" << std::endl;
		return -1;
	}

	// Define the viewport dimensions    
	glViewport(0, 0, WIDTH, HEIGHT);

	Shader myShader = Shader("vshader.glsl","fshader.glsl");
	Shader lightShader = Shader("light_vshader.glsl", "light_fshader.glsl");
	Shader simpleDepthShader = Shader("simpleDepthVShader.glsl", "simpleDepthFShader.glsl");

	Init();

	// Game loop    
	while (!glfwWindowShouldClose(window))
	{

		GLfloat currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// Check if any events have been activiated (key pressed, mouse moved etc.) and call corresponding response functions    
		glfwPollEvents();
		do_movement();

		glm::mat4 lightProjection, lightView;
		glm::mat4 lightSpaceMatrix;
		GLfloat near_plane = 1.0f, far_plane = 7.5f;
		lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
		lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
		lightSpaceMatrix = lightProjection * lightView;
		// - now render scene from light's point of view
		simpleDepthShader.use();
		glUniformMatrix4fv(glGetUniformLocation(simpleDepthShader.ID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		RenderScene(simpleDepthShader);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, WIDTH, HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		RenderScene(myShader);

		////light
		//lightShader.use();
		//// Get location objects for the matrices on the lamp shader (these could be different on a different shader)
		//modelLoc = glGetUniformLocation(lightShader.ID, "model");
		//viewLoc = glGetUniformLocation(lightShader.ID, "view");
		//projLoc = glGetUniformLocation(lightShader.ID, "projection");
		//// Set matrices
		//glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
		//glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
		//model = glm::translate(model, lightPos);
		//model = glm::scale(model, glm::vec3(0.2f));
		//glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		//// Draw the light object (using light's vertex attributes)
		//glBindVertexArray(lightVAO);
		//glDrawArrays(GL_TRIANGLES, 0, Positions.size());
		//glBindVertexArray(0);
		


		// Swap the screen buffers    
		glfwSwapBuffers(window);
	}

	// Terminate GLFW, clearing any resources allocated by GLFW.    
	glfwTerminate();
	return 0;
}

void RenderScene(Shader &shader)
{
	// Render    
	//glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

	shader.use();

	// Camera/View transformation
	glm::mat4 view(1.F);
	view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	view = glm::rotate(view, modelRotateX, glm::vec3(0.0f, 1.0f, 0.0f));
	//view = glm::rotate(view, modelRotateX, glm::vec3(0.5f, 0.0f, 0.5f));
	// Projection 
	glm::mat4 projection(1.f);
	projection = glm::perspective(fov, (GLfloat)WIDTH / (GLfloat)HEIGHT, 0.1f, 100.0f);
	// Get the uniform locations
	glm::mat4 model(1.f);
	GLint modelLoc = glGetUniformLocation(shader.ID, "model");
	GLint viewLoc = glGetUniformLocation(shader.ID, "view");
	GLint projLoc = glGetUniformLocation(shader.ID, "projection");
	// Pass the matrices to the shader
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));


	GLint objectColorLoc = glGetUniformLocation(shader.ID, "objectColor");
	GLint lightColorLoc = glGetUniformLocation(shader.ID, "lightColor");
	GLint lightPosLoc = glGetUniformLocation(shader.ID, "lightPos");
	GLint viewPosLoc = glGetUniformLocation(shader.ID, "viewPos");
	glUniform3f(viewPosLoc, cameraPos.x, cameraPos.y, cameraPos.z);
	glUniform3f(lightPosLoc, lightPos.x, lightPos.y, lightPos.z);
	glUniform3f(objectColorLoc, 1.0f, 0.5f, 0.31f);// 我们所熟悉的珊瑚红
	glUniform3f(lightColorLoc, 1.0f, 1.0f, 1.0f); // 依旧把光源设置为白色
	glUniform1i(glGetUniformLocation(shader.ID, "shadowMap"), 0);


	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, Positions.size());
	glBindVertexArray(0);

}

// Is called whenever a key is pressed/released via GLFW
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
	if (key >= 0 && key < 1024)
	{
		if (action == GLFW_PRESS)
			keys[key] = true;
		else if (action == GLFW_RELEASE)
			keys[key] = false;
	}
}

void do_movement()
{
	// Camera controls
	GLfloat cameraSpeed = 5.0f * deltaTime;
	if (keys[GLFW_KEY_W])
		cameraPos += cameraSpeed * cameraFront;
	if (keys[GLFW_KEY_S])
		cameraPos -= cameraSpeed * cameraFront;
	if (keys[GLFW_KEY_A])
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (keys[GLFW_KEY_D])
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

bool firstMouse = true;
bool isMBL = false;
bool isMBM = false;
bool isMBR = false;

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // Reversed since y-coordinates go from bottom to left
	lastX = xpos;
	lastY = ypos;

	if (isMBL)
	{
		GLfloat sensitivity = 0.1;	// Change this value to your liking
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		yaw += xoffset;
		pitch += yoffset;

		// Make sure that when pitch is out of bounds, screen doesn't get flipped
		

		glm::vec3 front;
		front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		cameraFront = glm::normalize(front);
	}
	else if (isMBR)
	{
		GLfloat sensitivity = 0.05;	// Change this value to your liking
		xoffset *= sensitivity;
		yoffset *= sensitivity;
		modelRotateX += xoffset;
		modelPotateY += yoffset;
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS) 
		switch (button)
		{
			case GLFW_MOUSE_BUTTON_LEFT:
				isMBL = true;
				break;
			case GLFW_MOUSE_BUTTON_MIDDLE:
				isMBM = true;
				break;
			case GLFW_MOUSE_BUTTON_RIGHT:
				isMBR = true;
				break;
			default:
				return;
		}
	else if(action == GLFW_RELEASE)
	{
		switch (button)
		{
		case GLFW_MOUSE_BUTTON_LEFT:
			isMBL = false;
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			isMBM = false;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			isMBR = false;
			break;
		default:
			return;
		}
	}
	return;
}
