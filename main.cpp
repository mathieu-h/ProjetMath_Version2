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

float height = 1200.0f;
float width = 1200.0f;

CPolygon polygon;
Window window;

//Menu indentifier
static int modify = 1 ;
static int drawMode = 1 ;

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

//Edge table
std::vector<Node<Edge>*> ET;

//Filling points
std::vector<std::vector<Point>> fillingPoints;


void draw_line(Point& a, Point& b)
{
    if(a.y_get() == b.y_get()){
        glColor3d(0, 0, 0);
        glBegin(GL_LINES);
        glVertex2f(a.x_get(),a.y_get());
        glVertex2f(b.x_get(),b.y_get());
        glEnd();
    }
}

float calculateSlope(Point a, Point b)
{
    if(std::fabs(b.x_get() - a.x_get()) < 0.01){
        return INFINITY;
    }
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
		currentEdge->SetNext(e);
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

	for (int i = 0; i < ET.size(); i++) {
		if(!ET[i]->data.isEmpty()){
			std::cout << i << " : ";
			Node<Edge>* node = ET[i];
			while(node->NextNode() != NULL)
			{
				std::cout << node->data;
				node = node->NextNode();
			}
			std::cout << node->data << std::endl;
		}
	}
}


Node<Edge>* InsertNodesIntoLCA(Node<Edge>* ptrLCA, int i){
	if(!ET[i]->data.isEmpty()){
		Node<Edge>* edgeET = ET[i];
		Node<Edge>* edgeLCA = NULL;
		Node<Edge>* currentEdgeLCA = NULL;
        edgeLCA = new Node<Edge>(edgeET->data);
        float x = edgeET->data.getXMin();
        if(!std::isinf(edgeLCA->data.getSlope())){
            x = (edgeLCA->data.getYMin() - edgeLCA->data.getYIntercept())/(edgeLCA->data.getSlope());
        }
        if(x < -1 || x > 1){
            std::cout << "BLA" << std::endl;
        }
        edgeLCA->data.setXMin(x);
        currentEdgeLCA = edgeLCA;
        
        while(edgeET->NextNode() != NULL)
        {
            edgeET = edgeET->NextNode();
            Node<Edge>* newEdgeLCA = new Node<Edge>(edgeET->data);
            float x = edgeET->data.getXMin();
            if(!std::isinf(newEdgeLCA->data.getSlope())){
                x = (newEdgeLCA->data.getYMin() - newEdgeLCA->data.getYIntercept())/(newEdgeLCA->data.getSlope());
            }
			newEdgeLCA->data.setXMin(x);
			currentEdgeLCA->SetNext(newEdgeLCA);
			//currentEdgeLCA = newEdgeLCA;
		}

		if(ptrLCA == NULL)
		{			
			ptrLCA = currentEdgeLCA;
		}
		else
		{
			Node<Edge>* currentNode = ptrLCA;
			while(currentNode->NextNode() != NULL)
			{ 
				currentNode = currentNode->NextNode();
			}
            currentNode->SetNext(currentEdgeLCA);
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
        // || ((currentNode->NextNode() != NULL) && moreThan2 && std::fabs(currentNode->data.getXMin() - currentNode->NextNode()->data.getXMin()) < 0.2)
        if (currentNode->data.getYMax() <= indexOpenGl/* || ((currentNode->NextNode() != NULL) && moreThan2 && std::fabs(currentNode->data.getXMin() - currentNode->NextNode()->data.getXMin()) < 0.1)*/)
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

Node<Edge>* RemoveImprecisions(Node<Edge>* ptrLCA){
    if(ptrLCA == NULL){
        return ptrLCA;
    }
    
    Node<Edge>* previousNode = ptrLCA;
    Node<Edge>* currentNode = ptrLCA;
    int cpt = 0;
    
    Node<Edge>* cptNode = ptrLCA;
    int cptNodes = 0;
    while(cptNode != NULL){
        cptNode = cptNode->NextNode();
        cptNodes++;
    }
    
    while(currentNode != NULL)
    {
        bool moreThan2 = (currentNode->NextNode()->NextNode() != NULL);
        if((currentNode->NextNode() != NULL) && moreThan2 && cptNodes%2 != 0 && std::fabs(currentNode->data.getXMin() - currentNode->NextNode()->data.getXMin()) < 0.01)
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
	Node<Edge>*	tail   = NULL;

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
		}else{
			tail->SetNext(next);
			//tail->InsertAfter(next);
		}
		tail = next;
	}
	return result;
}

int compare(Node<Edge>* one, Node<Edge>* two){
	float firstX = one->data.getXMin();
	float secondX = two->data.getXMin();
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
	Node<Edge>* currentNode = NULL;
	bool parity = 1;
	int cpt = 0;
	for(int i = 0 ; i < ET.size() ; i++){
		ptrLCA = InsertNodesIntoLCA(ptrLCA, i);
		if(ptrLCA != NULL){
            updateLCA(ptrLCA);
            ptrLCA = SortLCA(ptrLCA, &compare);
			ptrLCA = RemoveNodesFromLCA(ptrLCA, i);
            ptrLCA = RemoveImprecisions(ptrLCA);
            float indexOpenGL = convertViewportToOpenGLCoordinate(i/(float)glutGet(GLUT_WINDOW_HEIGHT));
            //currentNode = new Node<Edge>(*ptrLCA);
            currentNode = ptrLCA;
            std::vector<Point> vec;
            if(currentNode->NextNode() == NULL){
                std:cout << "BLAAAA";
            }
            while (currentNode){
                //if (parity){
                Point a(currentNode->data.getXMin(), indexOpenGL);
                //Point b(currentNode->NextNode()->data.getXMin(), indexOpenGL);
                vec.push_back(a);
                //vec.push_back(b);
                //std::cout << a << " " << b << std::endl;
                currentNode = currentNode->NextNode();
                //}
			}
			fillingPoints.push_back(vec);
		}
    }
    for(int i = 0; i < fillingPoints.size() ;++i)
    {
        std::cout << i << ": [ " << fillingPoints[i].size() << " ";
        for (int j = 0; j < fillingPoints[i].size(); j++)
        {
            std::cout << fillingPoints[i][j] << " , ";
        }
        std::cout << " ] " << std::endl;
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
			float new_x = convertViewportToOpenGLCoordinate(x/(float)glutGet(GLUT_WINDOW_WIDTH));

			float new_y = -convertViewportToOpenGLCoordinate(y/(float)glutGet(GLUT_WINDOW_HEIGHT));

            Point p(new_x, new_y);
            
            if(drawMode)
                polygon.addPoint(p);
            else
                window.add_point(p);
            
			//std::cout << p << std::endl;
		}
	}
	else if(button == GLUT_RIGHT_BUTTON)
	{
		if(state == GLUT_DOWN)
		{
			float index = convertViewportToOpenGLCoordinate(y/(float)glutGet(GLUT_WINDOW_HEIGHT));
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
    //Polygon
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

    //Window
	glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	glColor3d(0, 0, 0);
	glBegin(GL_POLYGON);

    points = window.get_points();
    
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

	glEnd();

    //LCA filling
	for (int i = 0; i < fillingPoints.size(); ++i)
	{
        if(fillingPoints[i].size() > 1){
            for (int j = 0; j < fillingPoints[i].size(); j++){
                if (j%2==0 && fillingPoints[i].size() > j+1){
                    draw_line(fillingPoints[i][j], fillingPoints[i][j + 1]);
                }
            }
        }
	}
}

void selectDraw(int selection) {
    switch (selection)
    {
        case 11 : drawMode = 1 ;
            break ;
        case 12 : drawMode = 0 ;
            break ;
    }
    glutPostRedisplay();
}

void selectModify(int selection) {
    switch (selection) {
        case 1  :
        case 2  :
        case 3  : modify = selection ;
            break ;
        case 0  : exit(0); }
    glutPostRedisplay();
}

void select(int selection) {
    switch (selection)
    {
        case 0  :
            exit(0);
    }
    glutPostRedisplay();
}

void renderScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(1, 1, 1, 1);
    
    
    //    DrawPolygon(window.get_points());
    DrawPolygon(polygon.get_points());

	glutSwapBuffers();
}

static GLfloat view_rotx = 20.0F ;
static GLfloat view_roty = 30.0F ;
static GLfloat view_rotz = 0.0F ;

void key(unsigned char key,int x,int y) {
    switch ( key ) {
        case 'z'    : view_rotz += 2.0;
            break;
        case 'Z'    : view_rotz -= 2.0;
            break;
        case '\033' : exit(0);
        break ; }
}

static void special(int k, int x, int y) {
    switch (k) {
        case GLUT_KEY_UP    : view_rotx += 2.0;
            break;
        case GLUT_KEY_DOWN  : view_rotx -= 2.0;
            break;
        case GLUT_KEY_LEFT  : view_roty += 2.0;
            break;
        case GLUT_KEY_RIGHT : view_roty -= 2.0;
        break; }
    glutPostRedisplay();
}


int main(int argc, char **argv) {

    // init GLUT and create Window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100,100);
	glutInitWindowSize(width,height);
	glutCreateWindow("Projet Math - GLUT");
    glEnable(GL_LINE_SMOOTH);

	// register callbacks
	glutDisplayFunc(renderScene);
	glutMouseFunc (MouseButton);
	glutIdleFunc(update);
	glutKeyboardFunc(keyPressed);
    
    int drawMenu = glutCreateMenu(selectDraw);
    glutAddMenuEntry("Polygon",11);
    glutAddMenuEntry("Window",12);
    int modifyMenu = glutCreateMenu(selectModify);
    glutAddMenuEntry("Windowing",1);
    glutAddMenuEntry("Filling",2);
    glutCreateMenu(select);
    glutAddSubMenu("Draw",drawMenu);
    glutAddSubMenu("Modify",modifyMenu);
    glutAddMenuEntry("Quitter",0);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
    //glutKeyboardFunc(key);
    glutSpecialFunc(special);

	// enter GLUT event processing cycle
	glutMainLoop();
	return 1;
}

#pragma endregion