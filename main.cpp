#include "stdafx.h"

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <numeric>
#include <limits>

#include "Point.h"
#include "CPolygon.h"
#include "Window.h"
#include "CVector.h"
#include "Edge.h"
#include "Node.h"


#ifdef __APPLE__
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#endif

float height = 640.0f;
float width = 640.0f;

CPolygon polygon;
Window window;

#pragma mark Windowing
#pragma region Windowing

float determinant(float matrix[2][2])
{
	return (matrix[0][0] * matrix[1][1]) - (matrix[0][1] * matrix[1][0]);
}


Point intersection(Point a, Point b, Point c, Point d)
{
	//matrice 1, matrice inverse.
	float matrixA[2][2];
	float matrixAReverse[2][2];
	float matrixRes[2];
	float matrixB[2];
	float det;

	float x1 = a.x_get();
	float x2 = b.x_get();
	float x3 = c.x_get();
	float x4 = d.x_get();

	float y1 = a.y_get();
	float y2 = b.y_get();
	float y3 = c.y_get();
	float y4 = d.y_get();

	Point p;

	matrixA[0][0] = (x2 - x1);
	matrixA[0][1] = (x3 - x4);
	matrixA[1][0] = (y2 - y1);
	matrixA[1][1] = (y3 - y4);

	matrixB[0] = (c.x_get() - a.x_get());
	matrixB[1] = (c.y_get()  - a.y_get() );

	det = determinant(matrixA);

	if(det == 0)
		throw 1;

	//Res = A-1 * B

	//The Matrix A must be reversed
	matrixAReverse[0][0] = matrixA[1][1] / det;
	matrixAReverse[0][1] = -matrixA[0][1] / det;
	matrixAReverse[1][0] = -matrixA[1][0] / det;
	matrixAReverse[1][1] = matrixA[0][0] / det;

	//Matrix product between reverse A and B matrix
	matrixRes[0] = matrixAReverse[0][0] * matrixB[0] + matrixAReverse[0][1] * matrixB[1];
	matrixRes[1] = matrixAReverse[1][0] * matrixB[0] + matrixAReverse[1][1] * matrixB[1];

	//The intersection is outise of the polygon current line segment
	if(matrixRes[0] > 1 || matrixRes[0] < 0)
		throw 2;

	//Calculate the intersection point
	p.x_set(((1 - matrixRes[0]) * x1) + (matrixRes[0] * x2));
	p.y_set(((1 - matrixRes[0]) * y1) + (matrixRes[0] * y2));

	return p;
}

/***  Determine if the point of the polygon is inside or outside the window   ***/
bool visible(Point lastPointPoly, Point currentPointWindow, Point nextPointWindow)
{
	//Vector in the same direction as the window's edge
	CVector vector_window(currentPointWindow, nextPointWindow);

	//Vector linking the window's edge and the polygon point
	CVector vector_poly(currentPointWindow, lastPointPoly);

	//We need the normal of the window edge vector
	CVector normal_window = vector_window.normal();

	//The dot product of the vector a . b = ax * bx + ay * by
	float dot_product = normal_window.diff_x() * vector_poly.diff_x() + normal_window.diff_y() * vector_poly.diff_y();

	return dot_product >= 0;
}

CPolygon windowing(const CPolygon polygon, const Window window)
{
	std::vector<Point> points_polygon = polygon.get_points();
	std::vector<Point> points_window = window.get_points();
	points_window.push_back(points_window.front());

	CPolygon polygonNew;

	for (std::size_t i = 0; i < points_window.size() - 1; ++i)
	{
		polygonNew.clearPoints();
		for (std::size_t j = 0; j <= points_polygon.size()-1; ++j)
		{

			if(j > 0)
			{
				try {
					Point intersectionPoint = intersection(points_polygon[j-1], points_polygon[j], points_window[i], points_window[i+1]);
					polygonNew.addPoint(intersectionPoint);
				}
				catch(int e)
				{

				}
			}
			if(visible(points_polygon[j], points_window[i], points_window[i+1]))
			{
				polygonNew.addPoint(points_polygon[j]);
			}
		}
		if(polygonNew.get_points().size() > 0)
		{
			try {
				Point intersectionPoint = intersection(points_polygon[points_polygon.size()-1], points_polygon[0], points_window[i], points_window[i+1]);
				polygonNew.addPoint(intersectionPoint);
			}
			catch(int e)
			{

			}
		}

		points_polygon = polygonNew.get_points();

	}

	polygonNew.set_points(points_polygon);
	return polygonNew;
}

#pragma endregion


#pragma mark Filling
#pragma region Filling

//Active Edge table
std::vector<Node<Edge>*> AET;

//Edge tabel
std::vector<Node<Edge>*> ET;

//Filling points
std::vector<std::vector<Point>> fillingPoints;


void draw_line(Point& a, Point& b)
{
	glColor3d(0, 0, 0);
	glBegin(GL_LINES);
	glVertex2f(a.x_get(),a.y_get());
	glVertex2f(b.x_get(),b.y_get());
	glEnd();
}

float calculateSlope(Point a, Point b)
{
	return (b.y_get() - a.y_get())/(b.x_get() - a.x_get());
}


float convertOpenGLToViewportCoordinate(float x)
{
	return (x + 1)/2;
}

float convertViewportToOpenGLCoordinate(float x)
{
	return (x * 2) - 1;
}


void InsertIntoEdgeTable(Node<Edge>* e, int index)
{
	Node<Edge>* currentEdge = ET[index];
	if(currentEdge->data.isEmpty()){
		ET[index] = e;
	}else{
		while(currentEdge->NextNode() != NULL){
			currentEdge = currentEdge->NextNode();
		}
		currentEdge->InsertAfter(e);
	}
}

void createEdgeTable(CPolygon const &polygon)
{
	Edge emptyEdge;

	for(std::size_t i = 0 ; i < glutGet(GLUT_WINDOW_HEIGHT) ; i++)
	{
		Edge e;
		Node<Edge>* node = new Node<Edge>(e);
		ET.insert(ET.begin() + i, node);
	}


	std::vector<Point> points = polygon.get_points();

	for (std::size_t i = 1; i <= points.size(); ++i)
	{
		Point start = points[i-1];
		Point end;
		if(i == points.size())
		{
			end = points[0];
		}
		else
		{
			end = points[i];
		}

		float slope = calculateSlope(start, end);
		if(slope == 0)
		{
			continue;
		}
		float yMax = (start.y_get() > end.y_get())?start.y_get():end.y_get();
		float yMin = (start.y_get() < end.y_get())?start.y_get():end.y_get();
		float xMin = (start.x_get() < end.x_get())?start.x_get():end.x_get();
		float yIntercept = (start.y_get() - (slope*start.x_get()));

		Edge edge(yMax, yMin, xMin, slope, yIntercept);
		Node<Edge>* node = new Node<Edge>(edge);

		float index = convertOpenGLToViewportCoordinate(yMin);
		index *= glutGet(GLUT_WINDOW_HEIGHT);
		int indexInt = (int) index;

		InsertIntoEdgeTable(node, indexInt);
	}

	/*for (int i = 0; i < ET.size(); i++) {
		if(!ET[i]->data.isEmpty()){
			std::cout << i << " : ";
			Node<Edge>* node = ET[i];
			while(node->NextNode() != NULL)
			{
				std::cout << node->data;
				node = node->NextNode();
			}
			std::cout << ET[i]->data << std::endl;
		}
	}*/
}


Node<Edge>* InsertNodesIntoLCA(Node<Edge>* ptrLCA, int i){
	if(!ET[i]->data.isEmpty()){
		Node<Edge>* edgeET = ET[i];
		Node<Edge>* edgeLCA = NULL;
		Node<Edge>* currentEdgeLCA = NULL;
		edgeLCA = new Node<Edge>(edgeET->data);
		currentEdgeLCA = edgeLCA;

		while(edgeET->NextNode() != NULL)
		{
			edgeET = edgeET->NextNode();
			Node<Edge>* newEdgeLCA = new Node<Edge>(edgeET->data);
			float x = (newEdgeLCA->data.getYMin() - newEdgeLCA->data.getYIntercept())/(newEdgeLCA->data.getSlope());
			newEdgeLCA->data.setXMin(x);
			currentEdgeLCA->InsertAfter(newEdgeLCA);
			currentEdgeLCA = newEdgeLCA;
		}

		if(ptrLCA == NULL)
		{			
			ptrLCA = edgeLCA;
		}
		else
		{
			Node<Edge>* currentNode = ptrLCA;
			while(currentNode->NextNode() != NULL)
			{ 
				currentNode = currentNode->NextNode();
			}
			currentNode->InsertAfter(edgeLCA);
		}
	}
	return ptrLCA;
}

Node<Edge>* RemoveNodesFromLCA(Node<Edge>* ptrLCA, float i){
	if(ptrLCA == NULL){
		return ptrLCA;
	}
	float indexOpenGl = convertViewportToOpenGLCoordinate(i/(float)glutGet(GLUT_WINDOW_HEIGHT));

	Node<Edge>* previousNode = ptrLCA;
	Node<Edge>* currentNode = ptrLCA;
	int cpt = 0;
	while(currentNode != NULL)
	{
		if(currentNode->data.getYMax() == i)
		{
			if(cpt == 0)
			{
				Node<Edge>* nodeToDelete = previousNode;
				previousNode = previousNode->NextNode();
				currentNode = previousNode->NextNode();
				ptrLCA = previousNode;
				delete nodeToDelete;
			}
			else
			{
				delete previousNode->DeleteAfter();
				currentNode = previousNode->NextNode();
				cpt++;
			}
		}
		else
		{
			previousNode = currentNode;
			currentNode = currentNode->NextNode();
			cpt++;
		}
	}

	return ptrLCA;
}

Node<Edge>* SortLCA(Node<Edge>* list,int (*compare)(Node<Edge>* one,Node<Edge>* two))
{
	// Trivial case.
	if (!list || !list->NextNode())
		return list;

	Node<Edge>* right = list;
	Node<Edge>*	temp  = list;
	Node<Edge>*	last  = list;
	Node<Edge>*	result = NULL;
	Node<Edge>*	next   = NULL;

	// Find halfway through the list (by running two pointers, one at twice the speed of the other).
	while (temp && temp->NextNode())
	{
		last = right;
		right = right->NextNode();
		temp = temp->NextNode()->NextNode();
	}

	// Break the list in two. (prev pointers are broken here, but we fix later)
	last->ClearNext();

	// Recurse on the two smaller lists:
	list = SortLCA(list, compare);
	right = SortLCA(right, compare);

	// Merge:
	while (list || right)
	{
		// Take from empty lists, or compare:
		if (!right) {
			next = list;
			list = list->NextNode();
		} else if (!list) {
			next = right;
			right = right->NextNode();
		} else if (compare(list, right) < 0) {
			next = list;
			list = list->NextNode();
		} else {
			next = right;
			right = right->NextNode();
		}		
		if (!result) {
			result=next;
		}
	}
	return result;
}

int compare(Node<Edge>* one, Node<Edge>* two){
	int firstX = one->data.getXMin();
	int secondX = one->data.getXMin();
	if(firstX < secondX){
		return -1;
	}
	else if( firstX == secondX){
		return 0;
	}
	else{
		return 1;
	}
}

Node<Edge>* updateLCA(Node<Edge>* ptrLCA)
{
	Node<Edge>* currentNode = ptrLCA;
	while(currentNode != NULL)
	{
		float incrementY = 2.0f/glutGet(GLUT_WINDOW_HEIGHT);
		currentNode->data.setXMin(currentNode->data.getXMin() + (incrementY/currentNode->data.getSlope()));
		currentNode = currentNode->NextNode();
	}
	return ptrLCA;
}

void FillingLCALoop(CPolygon const &polygon){
	createEdgeTable(polygon);
	Node<Edge>* ptrLCA = NULL;

	int cpt = 0;
	for(int i = 0 ; i < ET.size() ; i++){
		ptrLCA = InsertNodesIntoLCA(ptrLCA, i);
		if(ptrLCA != NULL){

			ptrLCA = RemoveNodesFromLCA(ptrLCA, i);
			//ptrLCA = SortLCA(ptrLCA, &compare);
			updateLCA(ptrLCA);
			float indexOpenGL = convertViewportToOpenGLCoordinate(i/(float)glutGet(GLUT_WINDOW_HEIGHT));
			Point a(ptrLCA->data.getXMin(), indexOpenGL);
			Point b(ptrLCA->NextNode()->data.getXMin(), indexOpenGL);
			std::vector<Point> vec;
			vec.push_back(a);
			vec.push_back(b);
			fillingPoints.push_back(vec);

			std::cout << a << " " << b << std::endl;
		}

	}

	//ptrLCA = RemoveNodesFromLCA(ptrLCA, j);
	//ptrLCA = SortLCA(ptrLCA, &compare);
	//std::cout << ptrLCA;
}


#pragma endregion

#pragma mark GLUT
#pragma region GLUT



void MouseButton(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON)
	{
		if(state == GLUT_DOWN)
		{
			//std::cout << x << " " << y << std::endl;
			float new_x = convertViewportToOpenGLCoordinate(x/(float)width);

			float new_y = -convertViewportToOpenGLCoordinate(y/(float)height);

			Point p(new_x, new_y);
			polygon.addPoint(p);
			//std::cout << p << std::endl;
		}
	}
	else if(button == GLUT_RIGHT_BUTTON)
	{
		if(state == GLUT_DOWN)
		{
			float index = convertViewportToOpenGLCoordinate(y/(float)height);
			index = convertOpenGLToViewportCoordinate(index);
			index *= glutGet(GLUT_WINDOW_HEIGHT);

			std::cout << y << std::endl;
		}
	}
}

void keyPressed(unsigned char key, int x, int y)
{
	if(key == 13)
	{
		CPolygon p = windowing(polygon, window);
		polygon = p;
	}else if(key == 'c')
	{
		polygon.clearPoints();
	}else if(key == 'f')
	{
		FillingLCALoop(polygon);
	}
}

void update()
{
	glutPostRedisplay();
}

void DrawPolygon(std::vector<Point> points)
{
	glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	glColor3d((float)(127.f/255.f), (float)(48.f/255.f), (float)(201.f/255.f));

	if(points.size() <= 2){

		glBegin(GL_LINES);
		for (std::size_t i = 0; i < points.size()  ; ++i) {
			glVertex2f(points[i].x_get(), points[i].y_get());
		}
		glEnd();
	}
	else
	{
		glBegin(GL_POLYGON);

		for (std::size_t i = 0; i < points.size()  ; ++i) {
			glVertex2f(points[i].x_get(), points[i].y_get());
		}
		glEnd();
	}

	glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	glColor3d(0, 0, 0);
	glBegin(GL_POLYGON);

	glVertex3f(-0.5, 0.5, 0.0);
	glVertex3f(0.5, 0.5, 0.0);
	glVertex3f(0.5, -0.5, 0.0);
	glVertex3f(-0.5, -0.5, 0.0);

	glEnd();

	for (int i = 0; i < fillingPoints.size(); ++i)
	{
		draw_line(fillingPoints[i][0], fillingPoints[i][1]);
	}

}

void renderScene()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(1, 1, 1, 1);


	//    DrawPolygon(window.get_points());
	DrawPolygon(polygon.get_points());

	glutSwapBuffers();
}

int main(int argc, char **argv) {

	Point p1(-0.5, 0.5);
	Point p2(0.5, 0.5);
	Point p3(0.5, -0.5);
	Point p4(-0.5, -0.5);
	Point p5(0.5, 1);
	Point p6(1, 0.5);
	Point p7(0.5, 0);


	window.add_point(p1);
	window.add_point(p2);
	window.add_point(p3);
	window.add_point(p4);

	// init GLUT and create Window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100,100);
	glutInitWindowSize(width,height);
	glutCreateWindow("Projet Math - GLUT");

	// register callbacks
	glutDisplayFunc(renderScene);
	glutMouseFunc (MouseButton);
	glutIdleFunc(update);
	glutKeyboardFunc(keyPressed);

	// enter GLUT event processing cycle
	glutMainLoop();
	return 1;
}

#pragma endregion