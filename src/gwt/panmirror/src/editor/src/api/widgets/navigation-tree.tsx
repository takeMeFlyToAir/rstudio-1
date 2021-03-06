/*
 * navigation-tree.tsx
 *
 * Copyright (C) 2020 by RStudio, PBC
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */

import React, { CSSProperties } from "react";

import { WidgetProps } from "./react";

import './navigation-tree.css';

// Individual nodes and children of the Select Tree
export interface NavigationTreeNode {
  key: string;
  name: string;
  image?: string;
  type: string;
  expanded?: boolean;
  children: NavigationTreeNode[];
}

interface NavigationTreeProps extends WidgetProps {
  height: number;
  nodes: NavigationTreeNode[];
  selectedNode?: NavigationTreeNode;
  nodeSelected: (node: NavigationTreeNode) => void;
}

// Indent level for each level
const kNavigationTreeIndent = 16;

// Select Tree is a single selection tree that is useful in 
// hierarchical navigation type contexts. It does not support
// multiple selection and is generally not a well behaved tree
// like you would use to navigate a hierarchical file system.
export const NavigationTree: React.FC<NavigationTreeProps> = props => {

  const style: CSSProperties = {
    overflowY: 'scroll',
    height: props.height + 'px',
    ...props.style
  };

  // Process keys to enable keyboard based navigation
  const processKey = (e: React.KeyboardEvent) => {
    const selectedNode = props.selectedNode;
    switch (e.key) {
      case 'ArrowDown':
        if (selectedNode) {
          const next = nextNode(selectedNode, props.nodes);
          props.nodeSelected(next);
        }
        break;

      case 'ArrowUp':
        if (selectedNode) {
          const previous = previousNode(selectedNode, props.nodes);
          props.nodeSelected(previous);
        }
        break;
    }
  };

  return (
    <div style={style} tabIndex={0} onKeyDown={processKey} >
      {props.nodes.map(treeNode => <NavigationTreeItem key={treeNode.key} node={treeNode} onSelected={props.nodeSelected} selectedNode={props.selectedNode} />)}
    </div>
  );
};


interface NavigationTreeItemProps extends WidgetProps {
  node: NavigationTreeNode;
  onSelected: (node: NavigationTreeNode) => void;
  indentLevel?: number;
  selectedNode?: NavigationTreeNode;
}

// Renders each item
const NavigationTreeItem: React.FC<NavigationTreeItemProps> = props => {

  // Select the tree node
  const onClick = (e: React.MouseEvent) => {
    e.preventDefault();
    e.stopPropagation();
    props.onSelected(props.node);
  };

  // Anytime the selected node changes, expand the selected node
  React.useEffect(() => {
    if (props.node === props.selectedNode) {
      props.node.expanded = true;
      setExpanded(props.node.expanded);
    }
  }, [props.selectedNode]);

  // Whether this node is expanded
  const [expanded, setExpanded] = React.useState<boolean>(props.node.expanded || false);

  // Whether this node is elected
  const selected = props.selectedNode && props.selectedNode === props.node;

  // Indent this node the proper amount
  const indentLevel = props.indentLevel || 0;
  const indentStyle = {
    paddingLeft: indentLevel * kNavigationTreeIndent + 'px'
  };

  const selectedClassName = `${selected ? 'pm-selected-navigation-tree-item' : 'pm-navigation-tree-item'} pm-navigation-tree-node`;
  return (
    <div key={props.node.key} onClick={onClick} style={props.style}>
      <div className={selectedClassName} style={indentLevel > 0 ? indentStyle : undefined}>
        {props.node.image ? <div className='pm-navigation-tree-node-image-div'><img src={props.node.image} alt={props.node.name} className='pm-navigation-tree-node-image' /></div> : null}
        <div className='pm-navigation-tree-node-label-div pm-text-color'>{props.node.name}</div>
      </div>
      {expanded ? props.node.children?.map(childNode => <NavigationTreeItem key={childNode.key} node={childNode} onSelected={props.onSelected} indentLevel={indentLevel + 1} selectedNode={props.selectedNode} />) : undefined}
    </div >
  );
};

// Indicates whether a given key is the identified node or one of its
// children
export function containsChild(key: string, node: NavigationTreeNode): boolean {
  if (node.key === key) {
    return true;
  }

  for (const childNode of node.children) {
    const hasChild = containsChild(key, childNode);
    if (hasChild) {
      return true;
    }
  }
  return false;
}

// Creates an ordered flattened list of visible nodes in the
// tree. Useful for incrementing through visible nodes :)
function visibleNodes(nodes: NavigationTreeNode[]) {

  const nodeList: NavigationTreeNode[][] = nodes.map(node => {
    if (node.expanded) {
      return [node].concat(visibleNodes(node.children));
    } else {
      return [node];
    }
  });
  return ([] as NavigationTreeNode[]).concat(...nodeList);

}

// Get the next node for the current node
function nextNode(node: NavigationTreeNode, allNodes: NavigationTreeNode[]): NavigationTreeNode {
  const nodes = visibleNodes(allNodes);
  const currentIndex = nodes.indexOf(node);
  if (currentIndex < nodes.length - 1) {
    return nodes[currentIndex + 1];
  } else {
    return nodes[0];
  }
}

// Get the previous node for the current node
function previousNode(node: NavigationTreeNode, allNodes: NavigationTreeNode[]): NavigationTreeNode {
  const nodes = visibleNodes(allNodes);
  const currentIndex = nodes.indexOf(node);
  if (currentIndex > 0) {
    return nodes[currentIndex - 1];
  } else {
    return nodes[nodes.length - 1];
  }

}