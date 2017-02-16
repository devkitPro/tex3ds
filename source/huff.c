#define COMPRESSION_INTERNAL
#include "compress.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct node_t node_t;

struct node_t
{
  node_t   *parent;
  node_t   *child[2];
  node_t   *next;
  size_t   count;
  uint32_t code;
  uint16_t pos;
  uint8_t  val;
  uint8_t  codelen;
};

void node_free(node_t *node)
{
  if(node->child[0])
    node_free(node->child[0]);
  if(node->child[1])
    node_free(node->child[1]);
  free(node);
}

static int
node_comparator(const void *lhs,
                const void *rhs)
{
  const node_t *lhs_node = *(const node_t **)lhs;
  const node_t *rhs_node = *(const node_t **)rhs;

  if(lhs_node->count < rhs_node->count)
    return -1;
  if(lhs_node->count > rhs_node->count)
    return 1;
  if(lhs_node->val < rhs_node->val)
    return -1;
  if(lhs_node->val > rhs_node->val)
    return 1;
  return 0;
}

#if 0
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

static void
print_nodes(node_t **nodes,
            size_t num_nodes,
            int    indent)
{
  for(size_t i = 0; i < num_nodes; ++i)
  {
    if(nodes[i])
      print_node(nodes[i], indent);
  }

  for(size_t i = 0; i < num_nodes; ++i)
  {
    if(nodes[i] && nodes[i]->child[0])
      print_nodes(nodes[i]->child, 2, indent+2);
  }
}
#endif

static void
print_code(uint32_t code,
           size_t   codelen)
{
  for(size_t i = 0; i < codelen; ++i)
    fputc('0' + ((code >> (codelen - i - 1)) & 1), stdout);
}

static void
print_tree(node_t   *node,
           uint32_t code,
           size_t   codelen)
{
  if(!node->child[0])
  {
    printf("0x%02x: ", node->val);
    print_code(code, codelen);
    fputc('\n', stdout);
  }
  else
  {
    print_tree(node->child[0], (code << 1) | 0, codelen + 1);
    print_tree(node->child[1], (code << 1) | 1, codelen + 1);
  }
}

static void
print_table(uint8_t  *tree,
            size_t   pos,
            uint32_t code,
            size_t   codelen)
{
  size_t offset = tree[pos] & 0x1F;
  size_t child  = (pos & ~1) + offset*2 + 2;

  if(!(tree[pos] & 0x80))
    print_table(tree, child+0, (code << 1) | 0, codelen + 1);
  if(!(tree[pos] & 0x40))
    print_table(tree, child+1, (code << 1) | 1, codelen + 1);

  if(tree[pos] & 0x80)
  {
    printf("0x%02x: ", tree[child+0]);
    print_code((code << 1) | 0, codelen + 1);
    fputc('\n', stdout);
  }
  if(tree[pos] & 0x40)
  {
    printf("0x%02x: ", tree[child+1]);
    print_code((code << 1) | 1, codelen + 1);
    fputc('\n', stdout);
  }
}

static void
build_codes(node_t   *node,
            uint32_t code,
            size_t   codelen)
{
  assert(codelen < 32);

  assert((node->child[0] && node->child[1])
      || (!node->child[0] && !node->child[1]));

  if(node->child[0])
  {
    build_codes(node->child[0], code << 1, codelen+1);
    build_codes(node->child[1], (code << 1) | 1, codelen+1);
  }
  else
  {
    node->code    = code;
    node->codelen = codelen;
  }
}

static void
build_lookup(node_t **nodes,
             node_t *n)
{
  if(n->child[0])
  {
    build_lookup(nodes, n->child[0]);
    build_lookup(nodes, n->child[1]);
  }
  else
    nodes[n->val] = n;
}

static size_t
node_count(node_t *n)
{
  if(n->child[0])
    return node_count(n->child[0]) + node_count(n->child[1]) + 1;
  return 1;
}

static size_t
leaf_count(node_t *n)
{
  if(n->child[0])
    return leaf_count(n->child[0]) + leaf_count(n->child[1]);
  return 1;
}

#define bitmap_t           uint32_t
#define BITMAP_WORD_SIZE   (CHAR_BIT*sizeof(bitmap_t))
#define BITMAP_WORDS(bits) (((bits)+(BITMAP_WORD_SIZE-1))/(BITMAP_WORD_SIZE))
#define BITMAP_ALLOC(bits) (bitmap_t*)calloc(BITMAP_WORDS(bits), sizeof(bitmap_t))

static inline void
bitmap_clear(bitmap_t *bitmap,
             size_t   bits)
{
  memset(bitmap, 0, sizeof(bitmap_t) * BITMAP_WORDS(bits));
}

static inline bool
bitmap_get(const bitmap_t *bitmap,
           size_t         bit)
{
  return (bitmap[bit/BITMAP_WORD_SIZE] >> (bit % BITMAP_WORD_SIZE)) & 1;
}

static inline void
bitmap_set(bitmap_t *bitmap,
           size_t   bit)
{
  assert(!(bitmap[bit/BITMAP_WORD_SIZE] & ((bitmap_t)1 << (bit % BITMAP_WORD_SIZE))));
  bitmap[bit/BITMAP_WORD_SIZE] |= ((bitmap_t)1 << (bit % BITMAP_WORD_SIZE));
}

static inline void
bitmap_reset(bitmap_t *bitmap,
             size_t   bit)
{
  assert(bitmap[bit/BITMAP_WORD_SIZE] & ((bitmap_t)1 << (bit % BITMAP_WORD_SIZE)));
  bitmap[bit/BITMAP_WORD_SIZE] &= ~((bitmap_t)1 << (bit % BITMAP_WORD_SIZE));
}

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

static void
encode_tree(uint8_t  *tree,
            node_t   *node,
            bitmap_t *bitmap)
{
  node_t  *tail = node;
  size_t  leaf, l_leaf, r_leaf;
  uint8_t mask;
  ssize_t next;

  assert(bitmap_get(bitmap, node->pos) == 1);

  leaf = leaf_count(node);
  if(leaf <= 64)
  {
    do
    {
      if(leaf == 1)
      {
        tree[node->pos] = node->val;
        printf("[0x%02x] data=0x%02x\n", node->pos, node->val);
      }
      else
      {
        mask = 0;

        if(leaf_count(node->child[0]) == 1)
          mask |= 0x80;
        if(leaf_count(node->child[1]) == 1)
          mask |= 0x40;

        next = bitmap_find(bitmap, 512, node->pos);
        assert(next > node->pos);
        assert(next % 2 == 0);
        assert(bitmap_get(bitmap, next+0) == 0);
        assert(bitmap_get(bitmap, next+1) == 0);
        assert((next - node->pos - 1)/2 < 64);

        tree[node->pos] = ((next - node->pos - 1)/2) | mask;
        printf("[0x%02x] 0x%02x, left=0x%02zx, right=0x%02zx\n",
               node->pos, tree[node->pos],
               next+0, next+1);

        bitmap_set(bitmap, next+0);
        node->child[0]->pos = next+0;
        bitmap_set(bitmap, next+1);
        node->child[1]->pos = next+1;

        tail->next = node->child[0];
        node->child[0]->next = node->child[1];
        tail = node->child[1];
      }

      node = node->next;
      if(node)
        leaf = leaf_count(node);
    } while(node != NULL);
  }
  else
  {
    mask = 0;

    l_leaf = leaf_count(node->child[0]);
    r_leaf = leaf_count(node->child[1]);

    if(l_leaf == 1)
      mask |= 0x80;
    if(r_leaf == 1)
      mask |= 0x40;

    next = bitmap_find(bitmap, 512, node->pos);
    assert(next > node->pos);
    assert(next % 2 == 0);
    assert(bitmap_get(bitmap, next+0) == 0);
    assert(bitmap_get(bitmap, next+1) == 0);
    assert((next - node->pos - 1)/2 < 64);

    tree[node->pos] = ((next - node->pos - 1)/2) | mask;
    printf("[0x%02x] 0x%02x, left=0x%02zx, right=0x%02zx\n",
           node->pos, tree[node->pos],
           next+0, next+1);

    bitmap_set(bitmap, next+0);
    node->child[0]->pos = next+0;
    bitmap_set(bitmap, next+1);
    node->child[1]->pos = next+1;

    if(l_leaf > 0x40)
      l_leaf = 0x40;

    assert(bitmap_get(bitmap, next+l_leaf*2+0) == 0);
    assert(bitmap_get(bitmap, next+l_leaf*2+1) == 0);

    bitmap_set(bitmap, next+l_leaf*2+0);
    bitmap_set(bitmap, next+l_leaf*2+1);

    encode_tree(tree, node->child[0], bitmap);

    assert(bitmap_get(bitmap, next+l_leaf*2+0) == 1);
    assert(bitmap_get(bitmap, next+l_leaf*2+1) == 1);

    bitmap_reset(bitmap, next+l_leaf*2+0);
    bitmap_reset(bitmap, next+l_leaf*2+1);

    encode_tree(tree, node->child[1], bitmap);
  }
}

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

  for(size_t i = 0; i < len; ++i)
    ++histogram[src[i]];

  for(size_t i = 0; i < 256; ++i)
  {
    if(histogram[i] > 0)
      ++num_nodes;
  }

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
      nodes[j] = (node_t*)calloc(1, sizeof(node_t));
      if(!nodes[j])
      {
        for(i = 0; i < j; ++i)
          node_free(nodes[i]);
        free(nodes);
        free(histogram);
        return NULL;
      }

      nodes[j]->val   = i;
      nodes[j]->count = histogram[i];
      ++j;
    }
  }

  free(histogram);

  while(num_nodes > 1)
  {
    qsort(nodes, num_nodes, sizeof(node_t*), node_comparator);

    node_t *node = (node_t*)calloc(1, sizeof(node_t));
    if(!node)
    {
      for(size_t i = 0; i < num_nodes; ++i)
        node_free(nodes[i]);
      free(nodes);
      return NULL;
    }

    node->child[0] = nodes[0];
    node->child[1] = nodes[1];
    node->count    = nodes[0]->count + nodes[1]->count;

    node->child[0]->parent = node;
    node->child[1]->parent = node;

    nodes[0] = node;
    nodes[1] = nodes[--num_nodes];
  }

  root = nodes[0];
  free(nodes);

  build_codes(root, code, codelen);

  return root;
}

typedef struct
{
  buffer_t *buffer;
  size_t   pos;
  uint32_t code;
  uint8_t  data[4];
} bitstream_t;

static inline void
bitstream_init(bitstream_t *stream,
               buffer_t    *buffer)
{
  stream->buffer = buffer;
  stream->pos    = 32;
  stream->code   = 0;
  memset(stream->data, 0, sizeof(stream->data));
}

static inline int
bitstream_flush(bitstream_t *stream)
{
  if(stream->pos < 32)
  {
    stream->data[0] = stream->code >>  0;
    stream->data[1] = stream->code >>  8;
    stream->data[2] = stream->code >> 16;
    stream->data[3] = stream->code >> 24;

    stream->pos  = 32;
    stream->code = 0;

    return buffer_push(stream->buffer, stream->data, sizeof(stream->data));
  }

  return 0;
}

static inline int
bitstream_push(bitstream_t *stream,
               uint32_t    code,
               size_t      len)
{
  for(size_t i = 1; i <= len; ++i)
  {
    --stream->pos;

    if(code & (1U << (len-i)))
      stream->code |= (1U << stream->pos);

    if(stream->pos == 0)
    {
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
  uint8_t       header[4];
  const uint8_t *src  = (const uint8_t*)source;
  node_t        *root = build_tree(src, len);
  node_t        **lookup; 
  uint8_t       *tree;
  bitmap_t      *bitmap;
  size_t        count;

  compression_header(header, 0x28, len);

  if(!root)
    return NULL;

  lookup = (node_t**)calloc(256, sizeof(node_t*));
  if(!lookup)
  {
    node_free(root);
    return NULL;
  }

  build_lookup(lookup, root);

  count = node_count(root);

  tree = (uint8_t*)calloc((count+2) & ~1, 1);
  if(!tree)
  {
    node_free(root);
    free(lookup);
    return NULL;
  }

  bitmap = BITMAP_ALLOC(512);
  if(!bitmap)
  {
    free(tree);
    node_free(root);
    free(lookup);
    return NULL;
  }

  bitmap_set(bitmap, 0);
  tree[0] = count/2;

  bitmap_set(bitmap, 1);
  root->pos = 1;
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

  buffer_init(&result);
  bitstream_init(&stream, &result);

  if(buffer_push(&result, header, sizeof(header)) != 0
  || buffer_push(&result, tree, (count+2) & ~1) != 0)
  {
    buffer_destroy(&result);
    free(tree);
    node_free(root);
    free(lookup);
    return NULL;
  }

  free(tree);
  
  for(size_t i = 0; i < len; ++i)
  {
    node_t *node = lookup[src[i]];
    if(bitstream_push(&stream, node->code, node->codelen) != 0)
    {
      buffer_destroy(&result);
      node_free(root);
      free(lookup);
      return NULL;
    }
  }

  node_free(root);
  free(lookup);

  if(bitstream_flush(&stream) != 0)
  {
    buffer_destroy(&result);
    return NULL;
  }

  if(buffer_pad(&result, 4) != 0)
  {
    buffer_destroy(&result);
    return NULL;
  }

  *outlen = result.len;
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
