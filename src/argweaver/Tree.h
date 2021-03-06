/*=============================================================================

  Matt Rasmussen
  Copyright 2010-2011

  Tree datastructure

=============================================================================*/

#ifndef ARGWEAVER_TREE_H
#define ARGWEAVER_TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <set>
#include <map>
#include <vector>

#include "ExtendArray.h"


/*

    Parent Array Format (ptree)


           4
          / \
         3   \
        / \   \
       /   \   \
       0   1    2

     Then the parent tree array representation is
       ptree = [3, 3, 4, 4, -1]
     such that ptree[node's id] = node's parent's id

     In addition, the following must be true
     1. tree must be binary: n leaves, n-1 internal nodes
     2. leaves must be numbered 0 to n-1
     3. internal nodes are numbered n to 2n-2
     4. root must be numbered 2n-2
     5. the parent of root is -1
     6. the length of ptree is 2n-1

*/

namespace spidir {

using namespace std;


// invariant: node->child->prev == last child of node
// invariant: [last child of node].next == NULL


// A node in the phylogenetic tree
class Node
{
public:
    Node(int nchildren=0) :
        name(-1),
        parent(NULL),
        children(NULL),
        nchildren(nchildren),
        dist(0.0)
    {
        if (nchildren != 0)
            allocChildren(nchildren);
    }

    ~Node()
    {
        if (children)
            delete [] children;
    }

    // Sets and allocates the number of children '_nchildren'
    void setChildren(int _nchildren)
    {
        children = resize(children, nchildren, _nchildren);
        nchildren = _nchildren;
    }

    // Allocates the number of children '_nchildren'
    void allocChildren(int _nchildren)
    {
        children = new Node* [_nchildren];
    }

    // Returns whether the node is a leaf
    bool isLeaf() const {
        return nchildren == 0;
    }

    // Adds a node 'node' to be a child
    void addChild(Node *node)
    {
        setChildren(nchildren + 1);
        children[nchildren - 1] = node;
        node->parent = this;

    }


    int name;           // node name id (matches index in tree.nodes)
    Node *parent;       // parent pointer
    Node **children;    // array of child pointers (size = nchildren)

    int nchildren;      // number of children
    double dist;         // branch length above node
    double age;
    string longname;    // node name (used mainly for leaves only)
};


class NodeMap {
public:
    NodeMap() {}
    NodeMap(const map<int,int> &nm) : nm(nm) {
        for (map<int,int>::const_iterator it=nm.begin(); it != nm.end(); ++it)
            inv_nm[it->second].insert(it->first);
    }
    NodeMap(const NodeMap &other) :
        nm(other.nm),
        inv_nm(other.inv_nm) {}
    NodeMap operator=(const NodeMap &other) {
        nm = other.nm;
        inv_nm = other.inv_nm;
        return other;
    }
    unsigned int size() {
        return nm.size();
    }
    void print() {
        printf("MAP map.size=%i inv_map.size=%i\n",
               (int)nm.size(), (int)inv_nm.size());
        for (unsigned int i=0; i < nm.size(); i++)
            printf("map[%i]=%i\n", i, nm[i]);
        printf("inverse map\n");
        for (map<int,set<int> >::iterator it=inv_nm.begin();
             it != inv_nm.end(); ++it) {
            printf("%i:", it->first);
            for (set<int>::iterator it2=it->second.begin();
                 it2 != it->second.end(); ++it2)
                printf(" %i", *it2);
            printf("\n");
        }
        fflush(stdout);
        return;
    }
    void remap_node(Node *n, int id, int *deleted_branch);
    void propogate_map(Node *n, int *deleted_branch, int count=0,
                       int count_since_change=0,
                       int maxcount=-1, int maxcount_since_change=3);

    map<int,int> nm;  //maps nodes in full tree to nodes in pruned tree
    map<int, set<int> > inv_nm;  //reverse
};

class NodeSpr;

// A phylogenetic tree
class Tree
{
public:
    Tree(int nnodes=0) :
        nnodes(nnodes),
        root(NULL),
        nodes(nnodes, 100)
    {
        for (int i=0; i<nnodes; i++)
            nodes[i] = new Node();
    }

    Tree(string newick, const vector<double>& times = vector<double>());

    virtual ~Tree()
    {
        for (int i=0; i<nnodes; i++)
            delete nodes[i];
    }

    // Sets the branch lengths of the tree
    //  Arguments:
    //      dists: array of lengths (size = nnodes)
    void setDists(double *dists)
    {
        for (int i=0; i<nnodes; i++)
            nodes[i]->dist = dists[i];
    }


    // Gets the branch lengths of the tree
    //  Arguments:
    //      dists: output array (size = nnodes) for storing branch lengths
    void getDists(double *dists)
    {
        for (int i=0; i<nnodes; i++)
            dists[i] = nodes[i]->dist;
    }

    // Sets the leaf names of the tree
    //  Arguments:
    //      names:      array of names (size > # leaves)
    //      leavesOnly: whether to only set names for leaves, or all nodes
    void setLeafNames(string *names, bool leavesOnly=true)
    {
        for (int i=0; i<nnodes; i++) {
            if (leavesOnly && !nodes[i]->isLeaf())
                nodes[i]->longname = "";
            else
                nodes[i]->longname = names[i];
        }
    }

    void reorderLeaves(string *names);

    void apply_spr(NodeSpr *spr, NodeMap *node_map=NULL);
    void update_spr(char *newick, const vector<double>& times = vector<double>());
    void update_spr_pruned(Tree *orig_tree);
    NodeMap prune(set<string> leafs, bool allBut=false);

    // Gets leaf names of the nodes of a tree
    // Internal nodes are often named "" (empty string)
    //  Arguments:
    //      names:      output array for storing node names (size > nnodes)
    //      leavesOnly: whether to only get names for leaves, or all nodes
    void getNames(string *names, bool leavesOnly=true)
    {
        for (int i=0; i<nnodes; i++)
            if (!leavesOnly || nodes[i]->isLeaf())
                names[i] = nodes[i]->longname;
    }

    // Returns whether tree is rooted
    bool isRooted()
    {
        return (root != NULL && root->nchildren == 2);
    }

    // Returns the pointer to a node given is name id 'name'
    Node *getNode(int name)
    {
        return nodes[name];
    }

    // Adds a node 'node' to the tree
    // This will also set the node's name id
    Node *addNode(Node *node)
    {
        nodes.append(node);
        node->name = nodes.size() - 1;
        nnodes = nodes.size();
        return node;
    }

    // Compute a topology hash of the tree
    //  Arguments:
    //      key: output array (size = nnodes) containing a unique sequence of
    //           integers for the tree
    void hashkey(int *key);

    bool sameTopology(Tree *other);

    // set the topology to match another tree
    void setTopology(Tree *other);

    // Roots the tree on branch 'newroot'
    void reroot(Node *newroot, bool onBranch=true);

    // Roots the tree on branch connecting 'node1' and 'node2'
    void reroot(Node *node1, Node *node2);

    // Removes rounding error by replacing times from newick string
    // with actual times from times.size() timepoints (by finding
    // closest time, must be within tol)
    // NOTE: times must be sorted!
    // Also note: assumes same distance to ancestral node from all child nodes
    void correct_times(const vector<double> &times, double tol=1);
    //void correct_times(map<string,double> times);

    double total_branchlength();
    double tmrca();
    double tmrca_half();
    double rth();
    double popsize();
    vector<double> coalCounts(vector<double> times);
    double num_zero_branches();
    double distBetweenLeaves(Node *n1, Node *n2);
    double distBetweenLeaves(string n1, string n2) {
        return distBetweenLeaves(nodes[nodename_map.find(n1)->second],
                                 nodes[nodename_map.find(n2)->second]);
    }
    set<Node*> lca(set<Node*> derived);

protected:
    //returns age1-age2 and asserts it is positive,
    //rounds up to zero if slightly neg
    double age_diff(double age1, double age2);
    string format_newick_recur(Node *n, bool internal_names=true,
                               char *branch_format_str=NULL,
                               const NodeSpr *spr=NULL,
                               bool oneline=true);

public:
    int get_node_from_newick(char *newick, char *nhx);
    string format_newick(bool internal_names=true,
                         bool branchlen=true, int num_decimal=5,
                         const NodeSpr *spr=NULL, bool oneline=true);
    void write_newick(FILE *f, bool internal_name=true, bool branchlen=true,
                      int num_decimal=5, const NodeSpr *spr=NULL,
                      bool oneline=true);

    // Returns a new copy of the tree
    Tree *copy();

    // Returns whether the tree is self consistent
    bool assertTree();

public:
    int nnodes;                 // number of nodes in tree
    Node *root;                 // root of the tree (NULL if no nodes)
    ExtendArray<Node*> nodes;   // array of nodes (size = nnodes)
    map<string,int> nodename_map;
};


//like Spr in local_tree.h, but with Node pointers and real times
class NodeSpr {
public:
    NodeSpr() : recomb_node(NULL), coal_node(NULL) {}
    NodeSpr(Tree *tree, char *newick,
            const vector<double> &times=vector<double>()) {
        update_spr_from_newick(tree, newick, times);
    }
    void correct_recomb_times(const vector<double> &times);
    void update_spr_from_newick(Tree *tree, char *newick_str,
                                const vector<double> &times=vector<double>());
    Node *recomb_node;
    Node *coal_node;
    double recomb_time;
    double coal_time;
};


// Efficient SPR operation on a tree and its pruned version
class SprPruned {
private:
    //update spr operation on pruned tree
    void update_spr_pruned();

    //update object by parsing newick string
    void update_slow(char *newick, const set<string> inds,
                     const vector<double> &times = vector<double>());
public:
    SprPruned(char *newick, const set<string> inds,
              const vector<double> &times = vector<double>())
        {
            orig_tree = pruned_tree = NULL;
            update_slow(newick, inds, times);
        }

    ~SprPruned() {
        delete orig_tree;
        if (pruned_tree != NULL) delete pruned_tree;
    }

    //print pruned tree if set, otherwise full tree, with NHX string giving
    // next SPR event
    string format_newick(bool internal_names=true,
                         bool branchlen=true, int num_decimal=5,
                         bool oneline=true) {
        if (pruned_tree != NULL)
            return pruned_tree->format_newick(internal_names,
                                              branchlen,
                                              num_decimal,
                                              &pruned_spr, oneline);
        return orig_tree->format_newick(internal_names, branchlen,
                                        num_decimal,
                                        &orig_spr, oneline);
    }

    //apply spr on both trees and get next SPR from newick string. Don't
    //parse the newick string unless previous SPR not set
    void update(char *newick, const set<string> inds,
                const vector<double> &times = vector<double>());

    Tree *orig_tree;
    Tree *pruned_tree;
    NodeSpr orig_spr;
    NodeSpr pruned_spr;
    NodeMap node_map;
};


// A hash function for a topology key to an integer
struct HashTopology {
    static unsigned int hash(const ExtendArray<int> &key)
    {
        unsigned int h = 0, g;

        for (int i=0; i<key.size(); i++) {
            h = (h << 4) + key[i];
            if ((g = h & 0xF0000000))
                h ^= g >> 24;
            h &= ~g;
        }

        return h;
    }
};


//=============================================================================
// Tree traversals

void getTreeSortedPostOrder(Tree *tree, ExtendArray<Node*> *nodes,
                            int *ordering, Node *node=NULL);
void getTreePostOrder(Tree *tree, ExtendArray<Node*> *nodes, Node *node=NULL);

void getTreePreOrder(Tree *tree, ExtendArray<Node*> *nodes, Node *node=NULL);


//=============================================================================
// Input/output

void printFtree(int nnodes, int **ftree);
void printTree(Tree *tree, Node *node=NULL, int depth=0);


// C exports
extern "C" {

    /*
// Creates a 'forward tree' from a 'parent tree'
void makeFtree(int nnodes, int *ptree, int ***ftree);

// Deallocates a 'forward tree'
void freeFtree(int nnodes, int **ftree);
    */

// Creates a tree object from a 'parent tree' array
void ptree2tree(int nnodes, int *ptree, Tree *tree);

// Creates a 'parent tree' from a tree object
void tree2ptree(Tree *tree, int *ptree);


Tree *makeTree(int nnodes, int *ptree);
void deleteTree(Tree *tree);
void setTreeDists(Tree *tree, double *dists);

}


} // namespace spidir


#endif // SPDIR_TREE_H

