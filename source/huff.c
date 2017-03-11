/*------------------------------------------------------------------------------
 * Copyright (c) 2017
 *     Michael Theall (mtheall)
 *
 * This file is part of tex3ds.
 *
 * tex3ds is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tex3ds is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tex3ds.  If not, see <http://www.gnu.org/licenses/>.
 *----------------------------------------------------------------------------*/
/** @file huff.c
 *  @brief Huffman compression routines
 */

/** @brief Enable internal compression routines */
#define COMPRESSION_INTERNAL
#include "compress.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/** @brief Huffman node */
typedef struct node_t node_t;

/** @brief Huffman node */
struct node_t
{
  node_t   *parent;   ///< Parent node
  node_t   *child[2]; ///< Children nodes
  node_t   *next;     ///< Node queue
  size_t   count;     ///< Node weight
  uint32_t code;      ///< Huffman encoding
  uint16_t pos;       ///< Huffman tree position
  uint8_t  val;       ///< Huffman tree value
  uint8_t  codelen;   ///< Huffman code length (bits)
};

/** @brief Free a node and its children
 *  @param[in] node Node to free
 */
void node_free(node_t *node)
{
  // free left subtree
  if(node->child[0])
    node_free(node->child[0]);

  // free right sub-tree
  if(node->child[1])
    node_free(node->child[1]);

  // free this node
  free(node);
}

/** @brief Node comparator
 *  @param[in] lhs Left side
 *  @param[in] rhs Right side
 *  @returns lhs < rhs
 *  @retval -1 when lhs < rhs
 *  @retval 0 when lhs == rhs
 *  @retval 1 when lhs > rhs
 */
static int
node_comparator(const void *lhs,
                const void *rhs)
{
  const node_t *lhs_node = *(const node_t **)lhs;
  const node_t *rhs_node = *(const node_t **)rhs;

  // major key is count
  if(lhs_node->count < rhs_node->count)
    return -1;
  if(lhs_node->count > rhs_node->count)
    return 1;

  // minor key is value
  if(lhs_node->val < rhs_node->val)
    return -1;
  if(lhs_node->val > rhs_node->val)
    return 1;

  // nodes are equal
  return 0;
}

#if 0
/** @brief Print node
 *  @param[in] node   Node to print
 *  @param[in] indent Indentation level
 */
static void
print_node(node_t *node,
           int    indent)
{
  if(node->child[0])
  {
    printf("%*s----: %zu\n", indent, "", node->count);
  }
  else
  {
    printf("%*s0x%02x: %zu ", indent, "", node->val, node->count);
    for(size_t j = 0; j < node->codelen; ++j)
      fputc('0' + ((node->code >> (node->codelen-j-1)) & 1), stdout);
    fputc('\n', stdout);
  }
}

/** @brief Print nodes and their children
 *  @param[in] nodes     Nodes to print
 *  @param[in] num_nodes Number of nodes
 *  @param[in] indent    Indentation level
 */
static void
print_nodes(node_t **nodes,
            size_t num_nodes,
            int    indent)
{
  // print each node
  for(size_t i = 0; i < num_nodes; ++i)
  {
    if(nodes[i])
      print_node(nodes[i], indent);
  }

  // print the children
  for(size_t i = 0; i < num_nodes; ++i)
  {
    if(nodes[i] && nodes[i]->child[0])
      print_nodes(nodes[i]->child, 2, indent+2);
  }
}
#endif

/** @brief Print a Huffman code
 *  @param[in] code    Huffman code
 *  @param[in] codelen Huffman code length (bits)
 */
static void
print_code(uint32_t code,
           size_t   codelen)
{
  for(size_t i = 0; i < codelen; ++i)
    fputc('0' + ((code >> (codelen - i - 1)) & 1), stdout);
}

/** @brief Print a Huffman tree
 *  @param[in] node    Node to print
 *  @param[in] code    Huffman code
 *  @param[in] codelen Huffman code length (bits)
 */
static void
print_tree(node_t   *node,
           uint32_t code,
           size_t   codelen)
{
  if(!node->child[0])
  {
    // this is a data node; print the data
    printf("0x%02x: ", node->val);
    print_code(code, codelen);
    fputc('\n', stdout);
  }
  else
  {
    // print the children
    print_tree(node->child[0], (code << 1) | 0, codelen + 1);
    print_tree(node->child[1], (code << 1) | 1, codelen + 1);
  }
}

/** @brief Print a Huffman table (encoded tree)
 *  @param[in] tree    Huffman table
 *  @param[in] pos     Position in table
 *  @param[in] code    Huffman code
 *  @param[in] codelen Huffman code length (bits)
 */
static void
print_table(uint8_t  *tree,
            size_t   pos,
            uint32_t code,
            size_t   codelen)
{
  size_t offset = tree[pos] & 0x1F;
  size_t child  = (pos & ~1) + offset*2 + 2;

  // print left subtree
  if(!(tree[pos] & 0x80))
    print_table(tree, child+0, (code << 1) | 0, codelen + 1);

  // print right subtree
  if(!(tree[pos] & 0x40))
    print_table(tree, child+1, (code << 1) | 1, codelen + 1);

  // print left data node
  if(tree[pos] & 0x80)
  {
    printf("0x%02x: ", tree[child+0]);
    print_code((code << 1) | 0, codelen + 1);
    fputc('\n', stdout);
  }

  // print right data node
  if(tree[pos] & 0x40)
  {
    printf("0x%02x: ", tree[child+1]);
    print_code((code << 1) | 1, codelen + 1);
    fputc('\n', stdout);
  }
}

/** @brief Build Huffman codes
 *  @param[in] node    Huffman node
 *  @param[in] code    Huffman code
 *  @param[in] codelen Huffman code length (bits)
 */
static void
build_codes(node_t   *node,
            uint32_t code,
            size_t   codelen)
{
  // don't exceept 32-bit codes
  assert(codelen < 32);

  // assert a full tree; every node has neither or both children
  assert((node->child[0] && node->child[1])
      || (!node->child[0] && !node->child[1]));

  if(node->child[0])
  {
    // build codes for each subtree
    build_codes(node->child[0], code << 1, codelen+1);
    build_codes(node->child[1], (code << 1) | 1, codelen+1);
  }
  else
  {
    // set code for data node
    node->code    = code;
    node->codelen = codelen;
  }
}

/** @brief Build lookup table
 *  @param[in] nodes Table to fill
 *  @param[in] n     Huffman node
 */
static void
build_lookup(node_t **nodes,
             node_t *n)
{
  if(n->child[0])
  {
    // build subtree lookups
    build_lookup(nodes, n->child[0]);
    build_lookup(nodes, n->child[1]);
  }
  else
  {
    // set lookup entry
    nodes[n->val] = n;
  }
}

/** @brief Count number of nodes in subtree
 *  @param[in] n Huffman node
 *  @returns Number of nodes in subtree
 */
static size_t
node_count(node_t *n)
{
  // sum of children plus self
  if(n->child[0])
    return node_count(n->child[0]) + node_count(n->child[1]) + 1;

  // this is a data node, just count self
  return 1;
}

/** @brief Count number of leaves in subtree
 *  @param[in] n Huffman node
 *  @returns Number of leaves in subtree
 */
static size_t
leaf_count(node_t *n)
{
  // sum of children
  if(n->child[0])
    return leaf_count(n->child[0]) + leaf_count(n->child[1]);

  // this is a data node; it is a leaf
  return 1;
}

/** @brief Bitmap element type */
typedef uint32_t bitmap_t;

/** @brief Number of bits per bitmap_t */
#define BITMAP_WORD_SIZE   (CHAR_BIT*sizeof(bitmap_t))

/** @brief Number of bitmap_t needed to represent some number of bits
 *  @param[in] bits Number of bits
 */
#define BITMAP_WORDS(bits) (((bits)+(BITMAP_WORD_SIZE-1))/(BITMAP_WORD_SIZE))

/** @brief Allocate a bitmap
 *  @param[in] bits Number of bits
 */
#define BITMAP_ALLOC(bits) (bitmap_t*)calloc(BITMAP_WORDS(bits), sizeof(bitmap_t))

/** @brief Clear a bitmap
 *  @param[in] bitmap Bitmap
 *  @param[in] bits   Number of bits
 */
static inline void
bitmap_clear(bitmap_t *bitmap,
             size_t   bits)
{
  memset(bitmap, 0, sizeof(bitmap_t) * BITMAP_WORDS(bits));
}

/** @brief Get a bit from a bitmap
 *  @param[in] bitmap Bitmap
 *  @param[in] bit    Bit to get
 *  @returns Bit
 */
static inline bool
bitmap_get(const bitmap_t *bitmap,
           size_t         bit)
{
  return (bitmap[bit/BITMAP_WORD_SIZE] >> (bit % BITMAP_WORD_SIZE)) & 1;
}

/** @brief Set a bit in a bitmap to 1
 *  @param[in] bitmap Bitmap
 *  @param[in] bit    Bit to set
 */
static inline void
bitmap_set(bitmap_t *bitmap,
           size_t   bit)
{
  assert(!(bitmap[bit/BITMAP_WORD_SIZE] & ((bitmap_t)1 << (bit % BITMAP_WORD_SIZE))));
  bitmap[bit/BITMAP_WORD_SIZE] |= ((bitmap_t)1 << (bit % BITMAP_WORD_SIZE));
}

/** @brief Reset a bit in a bitmap to 0
 *  @param[in] bitmap Bitmap
 *  @param[in] bit    Bit to reset
 */
static inline void
bitmap_reset(bitmap_t *bitmap,
             size_t   bit)
{
  assert(bitmap[bit/BITMAP_WORD_SIZE] & ((bitmap_t)1 << (bit % BITMAP_WORD_SIZE)));
  bitmap[bit/BITMAP_WORD_SIZE] &= ~((bitmap_t)1 << (bit % BITMAP_WORD_SIZE));
}

/** @brief Find a clear bit in a bitmap
 *  @param[in] bitmap Bitmap
 *  @param[in] bits   Number of bits in bitmap
 *  @param[in] pos    Minimum bit position
 *  @returns Index of first clear bit
 *  @retval -1 No clear bit found
 */
static inline ssize_t
bitmap_find(const bitmap_t *bitmap,
            size_t         bits,
            size_t         pos)
{
  for(size_t i = pos / BITMAP_WORD_SIZE; i < BITMAP_WORDS(bits); ++i)
  {
    if(bitmap[i] != ~(bitmap_t)0)
    {
      size_t j = 0;
      if(i == pos / BITMAP_WORD_SIZE)
        j = pos % BITMAP_WORD_SIZE;

      for(; j < BITMAP_WORD_SIZE && (i*BITMAP_WORD_SIZE+j) < bits; ++j)
      {
        if(!(bitmap[i] & ((bitmap_t)1 << j)))
          return i*BITMAP_WORD_SIZE + j;
      }
    }
  }

  return -1;
}

/** @brief Encode Huffman tree
 *  @param[in] tree   Huffman tree
 *  @param[in] node   Huffman node
 *  @param[in] bitmap Tree bitmap
 */
static void
encode_tree(uint8_t  *tree,
            node_t   *node,
            bitmap_t *bitmap)
{
  node_t  *tail = node;
  size_t  leaf, l_leaf, r_leaf;
  uint8_t mask;
  ssize_t next;

  // make sure this node's position is taken
  assert(bitmap_get(bitmap, node->pos) == 1);

  leaf = leaf_count(node);
  if(leaf <= 64)
  {
    do
    {
      if(leaf == 1)
      {
        // this is a data node; assign its value
        tree[node->pos] = node->val;
        printf("[0x%02x] data=0x%02x\n", node->pos, node->val);
      }
      else
      {
        // this is a branch node
        mask = 0;

        // check if left child is a data node
        if(leaf_count(node->child[0]) == 1)
          mask |= 0x80;

        // check if right child is a data node
        if(leaf_count(node->child[1]) == 1)
          mask |= 0x40;

        // find a free position in the tree
        next = bitmap_find(bitmap, 512, node->pos);
        assert(next > node->pos);
        assert(next % 2 == 0);
        assert(bitmap_get(bitmap, next+0) == 0);
        assert(bitmap_get(bitmap, next+1) == 0);
        assert((next - node->pos - 1)/2 < 64);

        // encode location/type of children nodes
        tree[node->pos] = ((next - node->pos - 1)/2) | mask;
        printf("[0x%02x] 0x%02x, left=0x%02zx, right=0x%02zx\n",
               node->pos, tree[node->pos],
               next+0, next+1);

        // mark the children positions as taken
        bitmap_set(bitmap, next+0);
        bitmap_set(bitmap, next+1);

        // set the children positions
        node->child[0]->pos = next+0;
        node->child[1]->pos = next+1;

        // append children to the node queue (breadth first process)
        tail->next = node->child[0];
        node->child[0]->next = node->child[1];
        tail = node->child[1];
      }

      // move to next node in queue
      node = node->next;
      if(node)
        leaf = leaf_count(node);
    } while(node != NULL);
  }
  else
  {
    // the subtree is large enough that the right child's children's positions
    // might be filled before we can encode them
    mask = 0;

    // get the left and right subtree leaf count
    l_leaf = leaf_count(node->child[0]);
    r_leaf = leaf_count(node->child[1]);

    // check if the left child is a data node
    if(l_leaf == 1)
      mask |= 0x80;

    // check if the right child is a data node
    if(r_leaf == 1)
      mask |= 0x40;

    // find a free position in the tree
    next = bitmap_find(bitmap, 512, node->pos);
    assert(next > node->pos);
    assert(next % 2 == 0);
    assert(bitmap_get(bitmap, next+0) == 0);
    assert(bitmap_get(bitmap, next+1) == 0);
    assert((next - node->pos - 1)/2 < 64);

    // encode location/type of children nodes
    tree[node->pos] = ((next - node->pos - 1)/2) | mask;
    printf("[0x%02x] 0x%02x, left=0x%02zx, right=0x%02zx\n",
           node->pos, tree[node->pos],
           next+0, next+1);

    // mark the children positions as taken
    bitmap_set(bitmap, next+0);
    bitmap_set(bitmap, next+1);

    // set the children positions
    node->child[0]->pos = next+0;
    node->child[1]->pos = next+1;

    // clamp position of right child's children
    if(l_leaf > 0x40)
      l_leaf = 0x40;

    // make sure right child's children have a free spot
    assert(bitmap_get(bitmap, next+l_leaf*2+0) == 0);
    assert(bitmap_get(bitmap, next+l_leaf*2+1) == 0);

    // reserve right child's children's position
    bitmap_set(bitmap, next+l_leaf*2+0);
    bitmap_set(bitmap, next+l_leaf*2+1);

    // encode left subtree
    encode_tree(tree, node->child[0], bitmap);

    // make sure right child's children still have their reservation
    assert(bitmap_get(bitmap, next+l_leaf*2+0) == 1);
    assert(bitmap_get(bitmap, next+l_leaf*2+1) == 1);

    // clear right child's children's reservation
    bitmap_reset(bitmap, next+l_leaf*2+0);
    bitmap_reset(bitmap, next+l_leaf*2+1);

    // encode right subtree
    encode_tree(tree, node->child[1], bitmap);
  }
}

/** @brief Build Huffman tree
 *  @param[in] src Source data
 *  @param[in] len Source data length
 *  @returns Root node
 */
static node_t*
build_tree(const uint8_t *src,
           size_t        len)
{
  size_t   *histogram = (size_t*)calloc(256, sizeof(size_t));
  size_t   num_nodes  = 0;
  uint32_t code       = 0;
  size_t   codelen    = 0;
  node_t   **nodes, *root;

  if(!histogram)
    return NULL;

  // fill in histogram
  for(size_t i = 0; i < len; ++i)
    ++histogram[src[i]];

  // count number of unique bytes
  for(size_t i = 0; i < 256; ++i)
  {
    if(histogram[i] > 0)
      ++num_nodes;
  }

  // allocate array of node pointers
  nodes = (node_t**)calloc(num_nodes, sizeof(node_t*));
  if(!nodes)
  {
    free(histogram);
    return NULL;
  }

  for(size_t i = 0, j = 0; i < 256; ++i)
  {
    if(histogram[i] > 0)
    {
      // allocate node
      nodes[j] = (node_t*)calloc(1, sizeof(node_t));
      if(!nodes[j])
      {
        // free the nodes
        for(i = 0; i < j; ++i)
          node_free(nodes[i]);
        free(nodes);
        free(histogram);
        return NULL;
      }

      // set node value
      nodes[j]->val = i;

      // set node count
      nodes[j]->count = histogram[i];
      ++j;
    }
  }

  // done with histogram
  free(histogram);

  // combine nodes
  while(num_nodes > 1)
  {
    // sort nodes by count; we will combine the two smallest nodes
    qsort(nodes, num_nodes, sizeof(node_t*), node_comparator);

    // allocate a parent node
    node_t *node = (node_t*)calloc(1, sizeof(node_t));
    if(!node)
    {
      for(size_t i = 0; i < num_nodes; ++i)
        node_free(nodes[i]);
      free(nodes);
      return NULL;
    }

    // left and right child are the two minimum nodes
    node->child[0] = nodes[0];
    node->child[1] = nodes[1];

    // node count is sum of children
    node->count    = nodes[0]->count + nodes[1]->count;

    // set children's parent to self
    node->child[0]->parent = node;
    node->child[1]->parent = node;

    // replace first node with self
    nodes[0] = node;

    // replace second node with last node
    nodes[1] = nodes[--num_nodes];
  }

  // root is the last node left
  root = nodes[0];

  // we are done with the array of node pointers
  free(nodes);

  // build Huffman codes
  build_codes(root, code, codelen);

  // return root node
  return root;
}

/** @brief Bitstream */
typedef struct
{
  buffer_t *buffer; ///< Output buffer
  size_t   pos;     ///< Bit position
  uint32_t code;    ///< Bitstream block
  uint8_t  data[4]; ///< Bitstream block buffer
} bitstream_t;

/** @brief Initialize bitstream
 *  @param[in] stream Bitstream to initialize
 *  @param[in] buffer Output buffer
 */
static inline void
bitstream_init(bitstream_t *stream,
               buffer_t    *buffer)
{
  stream->buffer = buffer;
  stream->pos    = 32;
  stream->code   = 0;
  memset(stream->data, 0, sizeof(stream->data));
}

/** @brief Flush bitstream block, padded to 32 bits
 *  @param[in] stream
 *  @retval 0  success
 *  @retval -1 failure
 */
static inline int
bitstream_flush(bitstream_t *stream)
{
  if(stream->pos < 32)
  {
    // this bitstream block has data
    stream->data[0] = stream->code >>  0;
    stream->data[1] = stream->code >>  8;
    stream->data[2] = stream->code >> 16;
    stream->data[3] = stream->code >> 24;

    // reset bitstream block
    stream->pos  = 32;
    stream->code = 0;

    // append bitstream block to output buffer
    return buffer_push(stream->buffer, stream->data, sizeof(stream->data));
  }

  return 0;
}

/** @brief Push Huffman code onto bitstream
 *  @param[in] stream Bitstream
 *  @param[in] code   Huffman code
 *  @param[in] len    Huffman code length (bits)
 *  @retval 0  success
 *  @retval -1 failure
 */
static inline int
bitstream_push(bitstream_t *stream,
               uint32_t    code,
               size_t      len)
{
  for(size_t i = 1; i <= len; ++i)
  {
    // get next bit position
    --stream->pos;

    // set/reset bit
    if(code & (1U << (len-i)))
      stream->code |= (1U << stream->pos);
    else
      stream->code &= ~(1U << stream->pos);

    if(stream->pos == 0)
    {
      // flush bitstream block
      if(bitstream_flush(stream) != 0)
        return -1;
    }
  }

  return 0;
}

void*
huff_encode(const void *source,
            size_t     len,
            size_t     *outlen)
{
  buffer_t      result;
  bitstream_t   stream;
  uint8_t       header[COMPRESSION_HEADER_SIZE];
  const uint8_t *src  = (const uint8_t*)source;
  node_t        *root = build_tree(src, len);
  node_t        **lookup;
  uint8_t       *tree;
  bitmap_t      *bitmap;
  size_t        count;

  // fill compression header
  compression_header(header, 0x28, len);

  if(!root)
    return NULL;

  // allocate lookup table
  lookup = (node_t**)calloc(256, sizeof(node_t*));
  if(!lookup)
  {
    node_free(root);
    return NULL;
  }

  // build lookup table
  build_lookup(lookup, root);

  // get number of nodes
  count = node_count(root);

  // allocate Huffman encoded tree
  tree = (uint8_t*)calloc((count+2) & ~1, 1);
  if(!tree)
  {
    node_free(root);
    free(lookup);
    return NULL;
  }

  // allocate bitmap
  bitmap = BITMAP_ALLOC(512);
  if(!bitmap)
  {
    free(tree);
    node_free(root);
    free(lookup);
    return NULL;
  }

  // first slot encodes tree size
  bitmap_set(bitmap, 0);
  tree[0] = count/2;

  // second slot encodes root node
  bitmap_set(bitmap, 1);
  root->pos = 1;

  // encode Huffman tree
  encode_tree(tree, root, bitmap);

#if 0
  for(size_t i = 0; i < count+1; ++i)
  {
    printf(" %02x", tree[i]);
    if(i % 8 == 7)
      fputc('\n', stdout);
  }
  fputc('\n', stdout);
#endif

  print_tree(root, 0, 0);
  printf("==============\n");
  print_table(tree, 1, 0, 0);

#if 0
  free(tree);
  node_free(root);
  free(lookup);
  return NULL;
#endif

  // initialize output buffer
  buffer_init(&result);

  // initialize bitstream
  bitstream_init(&stream, &result);

  // append compression header and Huffman tree to output data
  if(buffer_push(&result, header, sizeof(header)) != 0
  || buffer_push(&result, tree, (count+2) & ~1) != 0)
  {
    buffer_destroy(&result);
    free(tree);
    node_free(root);
    free(lookup);
    return NULL;
  }

  // we're done with the Huffman encoded tree
  free(tree);

  // encode each input byte
  for(size_t i = 0; i < len; ++i)
  {
    // lookup the byte value's node
    node_t *node = lookup[src[i]];

    // add Huffman code to bitstream
    if(bitstream_push(&stream, node->code, node->codelen) != 0)
    {
      buffer_destroy(&result);
      node_free(root);
      free(lookup);
      return NULL;
    }
  }

  // we're done with the Huffman tree and lookup table
  node_free(root);
  free(lookup);

  // flush the bitstream
  if(bitstream_flush(&stream) != 0)
  {
    buffer_destroy(&result);
    return NULL;
  }

  // pad the output buffer to 4 bytes
  if(buffer_pad(&result, 4) != 0)
  {
    buffer_destroy(&result);
    return NULL;
  }

  // set the output length
  *outlen = result.len;

  // return the output data
  return result.data;
}

void
huff_decode(const void *src,
            void       *dst,
            size_t     size)
{
  const size_t bits = 8;
  const uint8_t *in  = (const uint8_t*)src;
  uint8_t       *out = (uint8_t*)dst;
  uint32_t      treeSize = ((*in)+1)*2; // size of the huffman header
  uint32_t      word = 0;               // 32-bits of input bitstream
  uint32_t      mask = 0;               // which bit we are reading
  uint32_t      dataMask = (1<<bits)-1; // mask to apply to data
  const uint8_t *tree = in;             // huffman tree
  size_t        node;                   // node in the huffman tree
  size_t        child;                  // child of a node
  uint32_t      offset;                 // offset from node to child

  // point to the root of the huffman tree
  node = 1;

  // move input pointer to beginning of bitstream
  in += treeSize;

  while(size > 0)
  {
    if(mask == 0) // we exhausted 32 bits
    {
      // reset the mask
      mask = 0x80000000;

      // read the next 32 bits
      word = (in[0] <<  0)
           | (in[1] <<  8)
           | (in[2] << 16)
           | (in[3] << 24);
      in += 4;
    }

    // read the current node's offset value
    offset = tree[node] & 0x1F;

    child = (node & ~1) + offset*2 + 2;

    if(word & mask) // we read a 1
    {
      // point to the "right" child
      ++child;

      if(tree[node] & 0x40) // "right" child is a data node
      {
        // copy the child node into the output buffer and apply mask
        *out++ = tree[child] & dataMask;
        size--;

        // start over at the root node
        node = 1;
      }
      else // traverse to the "right" child
        node = child;
    }
    else // we read a 0
    {
      // pointed to the "left" child

      if(tree[node] & 0x80) // "left" child is a data node
      {
        // copy the child node into the output buffer and apply mask
        *out++ = tree[child] & dataMask;
        size--;

        // start over at the root node
        node = 1;
      }
      else // traverse to the "left" child
        node = child;
    }

    // shift to read next bit (read bit 31 to bit 0)
    mask >>= 1;
  }
}
