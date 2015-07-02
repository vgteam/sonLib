// Dynamically keeps track of the connected components in an
// undirected graph.
#include "sonLibGlobalsInternal.h"
#include <assert.h>

#define SEEN_TRUE (void*) 1
#define SEEN_FALSE (void*) 0

//Public data structures

struct _stConnectivity {
    // Data structure keeping track of the nodes, edges, and connected
    // components.
    stSet *nodes;
    stHash *seen;
    int nNodes;
	int nEdges;
    stList *et; //list of the Euler Tour Trees for each level in the graph
    int nLevels;
	stHash *connectedComponents;
	stEdgeContainer *edges;
	stEdgeContainer *incidentEdges;

};

struct _stConnectedComponent {
    // Data structure representing a connected component. It's OK
    // if this just contains an ID that indexes into a different data
    // structure or something--no need to keep the actual connected
    // component structure in here.

    // If the graph is modified, you can safely assume that the
    // outside code has discarded all the pointers to connected
    // components (please provide a destructor function so that the
    // memory can be freed in that case).
	void *nodeInComponent;
	stConnectivity *connectivity;

};

struct _stConnectedComponentIterator {
    // Iterator data structure for components in the graph.
	int cur;
	stConnectivity *connectivity;
	stList *componentList;
	stListIterator *componentIterator;
};

struct _stConnectedComponentNodeIterator {
    // Iterator for nodes in a component
	struct treap *currentTreapNode;
	stHash *seen;
	struct stEulerTourIterator *tourIterator;
};
//Private data structures
struct stDynamicEdge {
	void *edgeID;
	void *from;
	void *to;
	int in_forest;
	int enabled;
	int incident_from;
	int incident_to;
	int level;
};

//Node and edge representation
struct stDynamicEdge *stDynamicEdge_construct() {
	struct stDynamicEdge *edge = st_malloc(sizeof(struct stDynamicEdge));
	edge->from = NULL;
	edge->to = NULL;
	edge->in_forest = false;
	edge->enabled = true;
	edge->incident_from = false;
	edge->incident_to = false;
	edge->level = 0;
	return(edge);
}
void stDynamicEdge_destruct(struct stDynamicEdge *edge) {
	free(edge);
}


// Exported methods ----------------------------------------------------------------------

stConnectivity *stConnectivity_construct(void) {
    // Initialize the data structure with an empty graph with 0 nodes.
    stConnectivity *connectivity = st_malloc(sizeof(stConnectivity));
    connectivity->seen = stHash_construct();
	connectivity->nodes = stSet_construct();

    connectivity->et = stList_construct3(0, (void(*)(void*))stEulerTour_destruct);
	
	//add level zero Euler Tour
	stList_append(connectivity->et, stEulerTour_construct());

    connectivity->nLevels = 0;
    connectivity->nNodes = 0;
	connectivity->nEdges = 0;
	connectivity->connectedComponents =
		stHash_construct2(NULL, (void(*)(void*))stConnectedComponent_destruct);
	connectivity->edges = stEdgeContainer_construct2((void(*)(void*))stDynamicEdge_destruct);
	connectivity->incidentEdges = stEdgeContainer_construct();

    return(connectivity);

}

void stConnectivity_destruct(stConnectivity *connectivity) {
    // Free the memory for the data structure.
	stList_destruct(connectivity->et);
	stSet_destruct(connectivity->nodes);
	stHash_destruct(connectivity->connectedComponents);
	stEdgeContainer_destruct(connectivity->edges);
	stEdgeContainer_destruct(connectivity->incidentEdges);

    stHash_destruct(connectivity->seen);
	free(connectivity);
}

	

//add new levels to compensate for the addition of new nodes,
//and copy nodes from level 0 to the new levels so that all
//levels have the same nodes
void resizeEulerTourList(stConnectivity *connectivity) {
	if (connectivity->nLevels <= stList_length(connectivity->et)) {
		return;
	}
	while(stList_length(connectivity->et) < connectivity->nLevels) {
		struct stEulerTour *newLevel = stEulerTour_construct();
		stSetIterator *it = stSet_getIterator(connectivity->nodes);
		void *node;
		while((node = stSet_getNext(it))) {
			stEulerTour_createVertex(newLevel, node);
		}
		stList_append(connectivity->et, newLevel);
		stSet_destructIterator(it);
	}
}
void resizeIncidentEdgeList(stList *incident, int newsize) {
	for(int i = newsize; i < stList_length(incident); i++) {
		stList_remove(incident, newsize);
	}
}
	
void stConnectivity_addNode(stConnectivity *connectivity, void *node) {
    // Add an isolated node to the graph. This should end up in a new
    // connected component with only one member.
	connectivity->nNodes++;
	connectivity->nLevels = (int) ((int)(log(connectivity->nNodes)/log(2)) + 1);
    resizeEulerTourList(connectivity);


	stSet_insert(connectivity->nodes, node);
	stHash_insert(connectivity->seen, node, SEEN_FALSE);
	
	stHash_insert(connectivity->connectedComponents, node, 
			stConnectedComponent_construct(connectivity, node));
	//stEdgeContainer_addNode(connectivity->edges, node);

    for(int i = 0; i < connectivity->nLevels; i++) {
		//Add a new disconnected node to the Euler tour on level i
		struct stEulerTour *et_i = stList_get(connectivity->et, i);
		stEulerTour_createVertex(et_i, node);
	}
	
}
struct stDynamicEdge *stConnectivity_getEdge(stConnectivity *connectivity, void *node1, void *node2) {
	struct stDynamicEdge *edge = stEdgeContainer_getEdge(connectivity->edges, node1, node2);
	if(edge == NULL) {
		edge = stEdgeContainer_getEdge(connectivity->edges, node2, node1);
	}
	return(edge);
}

void stConnectivity_addEdge(stConnectivity *connectivity, void *node1, void *node2) {
    // Add an edge to the graph and update the connected components.
    // NB: The ordering of node1 and node2 shouldn't matter as this is an undirected graph.
	if(node1 == node2) return;
	connectivity->nEdges++;
	struct stDynamicEdge *newEdge = stDynamicEdge_construct();
	newEdge->from = node1;
	newEdge->to = node2;
	newEdge->level = connectivity->nLevels - 1;
	stEdgeContainer_addEdge(connectivity->edges, node1, node2, newEdge);

	struct stEulerTour *et_lowest = stList_get(connectivity->et, connectivity->nLevels - 1);
	if(!stEulerTour_connected(et_lowest, node1, node2)) {
		//remove the connected component for node 2 from the list of connected components. The
		//component that now contains node1 and node2 will have a pointer to a node in node1's
		//original component before the merge
		stConnectedComponent *node2Component = stConnectivity_getConnectedComponent(connectivity,
				node2);
		stHash_remove(connectivity->connectedComponents, node2Component->nodeInComponent);
		stConnectedComponent_destruct(node2Component);

		stEulerTour_link(et_lowest, node1, node2);
		newEdge->in_forest = true;
		return;
	}
	else {
		newEdge->in_forest = false;
		if(!newEdge->incident_from) {
			newEdge->incident_from = true;
			stEdgeContainer_addEdge(connectivity->incidentEdges, node1, node2, node2);


		}
		if(!newEdge->incident_to) {
			newEdge->incident_to = true;
			stEdgeContainer_addEdge(connectivity->incidentEdges, node2, node1, node1);

		}
	}

}

int stConnectivity_connected(stConnectivity *connectivity, void *node1, void *node2) {
	struct stEulerTour *et_lowest = stList_get(connectivity->et, connectivity->nLevels - 1);
	return(stEulerTour_connected(et_lowest, node1, node2));
}

void print_list(stList *list) {
	stListIterator *it = stList_getIterator(list);
	void *item;
	while((item = stList_getNext(it))) {
		printf("%p ", item);
	}
	stList_destructIterator(it);
	printf("\n");
}
struct stDynamicEdge *visit(stConnectivity *connectivity, void *w, void *otherTreeVertex, 
		struct stDynamicEdge *removedEdge, int level) {
	if(stHash_search(connectivity->seen, w) == SEEN_TRUE) {
		return(NULL);
	}
	stHash_insert(connectivity->seen, w, SEEN_TRUE);
	stList *w_incident = stEdgeContainer_getIncidentEdgeList(connectivity->incidentEdges, w);
	int w_incident_length = stList_length(w_incident);

	int k, j = 0;

	struct stEulerTour *et_level = stList_get(connectivity->et, level);
	for(k = 0; k < w_incident_length; k++) {
		void *e_wk_node2 = stList_get(w_incident, k);
		struct stDynamicEdge *e_wk = stConnectivity_getEdge(connectivity, 
				w, e_wk_node2);
		if (e_wk == removedEdge || e_wk->in_forest || !e_wk->enabled) {
			//remove
			if(w == e_wk->from) {
				e_wk->incident_from = false;
			}
			else {
				e_wk->incident_to = false;
			}
			continue;
		}
		if (e_wk->level == level) {
			void *otherNode = e_wk->from == w ? e_wk->to: e_wk->from;
			if (stEulerTour_connected(et_level, otherTreeVertex, otherNode)) {
				//remove e_wk
				if(w == e_wk->from) {
					e_wk->incident_from = false;
				}
				else {
					e_wk->incident_to = false;
				}
				for(k = k+1; k < w_incident_length; k++) {
					struct stDynamicEdge *e_toRemove_node2 = stList_get(w_incident, k);
					struct stDynamicEdge *e_toRemove = stConnectivity_getEdge(connectivity, 
							w, e_toRemove_node2);
					if(e_toRemove == removedEdge || e_toRemove->in_forest || 
							!e_toRemove->enabled) {
						if (w == e_toRemove->from) {
							e_toRemove->incident_from = false;
						}
						else {
							e_toRemove->incident_to = false;
						}
						continue;
					}
					stList_set(w_incident, j++, e_toRemove_node2);

				}
				resizeIncidentEdgeList(w_incident, j);
				return(e_wk);
			}
			else {
				e_wk->level--;
			}
		}
		stList_set(w_incident, j++, e_wk_node2);

	}
	resizeIncidentEdgeList(w_incident, j);
	stList_destruct(w_incident);
	return(NULL);

}

bool stConnectivity_removeEdge(stConnectivity *connectivity, void *node1, void *node2) {
	stConnectedComponent *componentWithBothNodes = 
		stConnectivity_getConnectedComponent(connectivity, node1);
	struct stDynamicEdge *edge = stConnectivity_getEdge(connectivity, 
			node1, node2);
	if(!edge) return(false);
	if(!edge->in_forest) {
		return(true);
	}
	struct stDynamicEdge *replacementEdge = NULL;
	bool componentsDisconnected = true;
	for (int i = edge->level; !replacementEdge && i < connectivity->nLevels; i++) {
		struct stEulerTour *et_i = stList_get(connectivity->et, i);
		stEulerTour_cut(et_i, node1, node2);

		//set node1 equal to id of the vertex in the smaller of the two components that have just
		//been created by deleting the edge
		if(stEulerTour_size(et_i, node2) > stEulerTour_size(et_i, node1)) {
			void *temp = node1;
			node1 = node2; 
			node2 = temp;
		}
		edge->in_forest = false;
		replacementEdge = visit(connectivity, node2, node1, edge, i);

		//go through each edge in the tour on level i
		struct stEulerHalfEdge *tourEdge = stEulerTour_getFirstEdge(et_i, node2);
		while(tourEdge) {
			struct stDynamicEdge *treeEdge = stConnectivity_getEdge(connectivity, 
					tourEdge->from->vertexID, tourEdge->to->vertexID);
			if(treeEdge->level == i) {
				treeEdge->level--;
				struct stEulerTour *et_te = stList_get(connectivity->et, treeEdge->level);
				stEulerTour_link(et_te, treeEdge->from, treeEdge->to);
			}
			for (int n = 0; !replacementEdge && n < 2; n++) {
				void *w = n ? treeEdge->to : treeEdge->from;
				replacementEdge = visit(connectivity, w, node1, edge, i);
			}

			tourEdge = stEulerTour_getNextEdgeInTour(et_i, tourEdge);
		}
		stHash_insert(connectivity->seen, node2, SEEN_FALSE);

		tourEdge = stEulerTour_getFirstEdge(et_i, node2);
		while(tourEdge) {
			stHash_insert(connectivity->seen, tourEdge->from, SEEN_FALSE);
			stHash_insert(connectivity->seen, tourEdge->to, SEEN_FALSE);
			tourEdge = stEulerTour_getNextEdgeInTour(et_i, tourEdge);
		}
		if(replacementEdge) {
			componentsDisconnected = false;
			assert(replacementEdge->level == i);
			stEulerTour_link(et_i, replacementEdge->from, replacementEdge->to);
			replacementEdge->in_forest = true;
			for(int h = replacementEdge->level + 1; h < connectivity->nLevels; h++) {
				struct stEulerTour *et_h = stList_get(connectivity->et, h);
				stEulerTour_cut(et_h, node1, node2);
				stEulerTour_link(et_h, replacementEdge->from, replacementEdge->to);
			}
		}
	}
	if(componentsDisconnected) {
		//remove the old component containing both node1 and node2 from the component hash. This
		//large component will be indexed by either a node from node1's new component or node2's
		//new component
		stHash_remove(connectivity->connectedComponents, componentWithBothNodes->nodeInComponent);
		stConnectedComponent_destruct(componentWithBothNodes);
		stHash_insert(connectivity->connectedComponents, node1,
				stConnectedComponent_construct(connectivity, node1));
		stHash_insert(connectivity->connectedComponents, node2,
				stConnectedComponent_construct(connectivity, node2));
	}
	edge->level = connectivity->nLevels;
	stEdgeContainer_deleteEdge(connectivity->edges, node1, node2);
	stEdgeContainer_deleteEdge(connectivity->edges, node2, node1);
	return(true);

}
struct stEulerTour *stConnectivity_getTopLevel(stConnectivity *connectivity) {
	return(stList_get(connectivity->et, connectivity->nLevels - 1));
}

// Might be cool to be able to add or remove several edges at once, if there is a way to
// make that more efficient than adding them one at a time. Depends on the details of 
// the algorithm you use.

void stConnectivity_removeNode(stConnectivity *connectivity, void *node) {
    // Remove a node (and all its edges) from the graph and update the connected components.
}
stConnectedComponent *stConnectedComponent_construct(stConnectivity *connectivity, void *node) {
	stConnectedComponent *component = st_malloc(sizeof(stConnectedComponent));
	component->nodeInComponent = node;
	component->connectivity = connectivity;
	return(component);
}

stConnectedComponent *stConnectivity_getConnectedComponent(stConnectivity *connectivity, 
		void *node) {
    // Get the connected component that this node is a member of. If
    // the graph is modified, this pointer can be invalidated
	stConnectedComponent *comp = stConnectedComponent_construct(connectivity, node);
	stConnectedComponentNodeIterator *it = stConnectedComponent_getNodeIterator(comp);
	stConnectedComponent *foundComponent = NULL;
	while(node) {
		foundComponent = stHash_search(connectivity->connectedComponents, node);
		if(foundComponent) {
			break;
		}
		node = stConnectedComponentNodeIterator_getNext(it);

	}
	stConnectedComponentNodeIterator_destruct(it);
	stConnectedComponent_destruct(comp);
			
	return(foundComponent);
}

stConnectedComponentNodeIterator *stConnectedComponent_getNodeIterator(stConnectedComponent 
		*component) {
    // Get an iterator over the nodes in a particular connected
    // component. You can safely assume that the graph won't be
    // modified while this iterator is active.
	stConnectedComponentNodeIterator *it = st_malloc(sizeof(stConnectedComponentNodeIterator));
	struct stEulerTour *et = stConnectivity_getTopLevel(component->connectivity);
	it->seen = stHash_construct();
	it->tourIterator = stEulerTour_getIterator(et, component->nodeInComponent);
	return(it);
}

void *stConnectedComponentNodeIterator_getNext(stConnectedComponentNodeIterator *it) {
    // Return the next node of the connected component, or NULL if all have been traversed.
	//void *currentNode = it->currentNode;
	void *currentNode = stEulerTourIterator_getNext(it->tourIterator);

	if(stHash_search(it->seen, currentNode)) {
		return(stConnectedComponentNodeIterator_getNext(it));
	}
	stHash_insert(it->seen, currentNode, SEEN_TRUE);
	return(currentNode);

}

void stConnectedComponentNodeIterator_destruct(stConnectedComponentNodeIterator *it) {
    // Free the iterator data structure.
	stHash_destruct(it->seen);
	stEulerTourIterator_destruct(it->tourIterator);
	free(it);
}

stConnectedComponentIterator *stConnectivity_getConnectedComponentIterator(stConnectivity *connectivity) {
    // Get an iterator over the connected components in the
    // graph. Again, if the graph is modified while the iterator is
    // active, it's ok for it to break.
	stConnectedComponentIterator *it = st_malloc(sizeof(stConnectedComponentIterator));
	it->componentList = stHash_getValues(connectivity->connectedComponents);
	it->componentIterator = stList_getIterator(it->componentList);
	it->connectivity = connectivity;
	return(it);
}

stConnectedComponent *stConnectedComponentIterator_getNext(stConnectedComponentIterator *it) {
    // Return the next connected component in the graph, or NULL if all have been traversed.
	stConnectedComponent *next = stList_getNext(it->componentIterator);
	return(next);
}

void stConnectedComponentIterator_destruct(stConnectedComponentIterator *it) {
    // Free the iterator data structure.
	stList_destructIterator(it->componentIterator);
	stList_destruct(it->componentList);
	free(it);
}
void stConnectedComponent_destruct(stConnectedComponent *comp) {
	free(comp);
}

