import 'jasmine';
import { firstElementId, findElementByKey } from './snapshot';

describe('snapshot tree helpers', () => {
  describe('firstElementId', () => {
    it('returns null for null/non-object input', () => {
      expect(firstElementId(null)).toBeNull();
      expect(firstElementId(undefined)).toBeNull();
      expect(firstElementId(42)).toBeNull();
      expect(firstElementId('string')).toBeNull();
    });

    it('returns the element id from a direct element node', () => {
      const node = { element: { id: 5 } };
      expect(firstElementId(node)).toBe('5');
    });

    it('returns the first element id from nested children', () => {
      const node = {
        children: [
          { tag: 'view' },
          { children: [{ element: { id: 42 } }] },
        ],
      };
      expect(firstElementId(node)).toBe('42');
    });

    it('returns null when no element is found', () => {
      const node = { children: [{ tag: 'view' }, { tag: 'label' }] };
      expect(firstElementId(node)).toBeNull();
    });

    it('handles element id of 0', () => {
      const node = { element: { id: 0 } };
      expect(firstElementId(node)).toBe('0');
    });
  });

  describe('findElementByKey', () => {
    it('finds element by explicit key', () => {
      const tree = {
        children: [
          { key: 'header', element: { id: 10 } },
          { key: 'body', element: { id: 20 } },
        ],
      };
      expect(findElementByKey(tree, 'header')).toBe('10');
      expect(findElementByKey(tree, 'body')).toBe('20');
    });

    it('finds element by tag name', () => {
      const tree = {
        children: [
          { tag: 'ChatFriendsList', element: { id: 15 } },
        ],
      };
      expect(findElementByKey(tree, 'ChatFriendsList')).toBe('15');
    });

    it('finds component node by tag and returns first child element id', () => {
      const tree = {
        children: [
          {
            tag: 'MyComponent',
            component: 'MyComponent',
            children: [
              { element: { id: 99 } },
            ],
          },
        ],
      };
      expect(findElementByKey(tree, 'MyComponent')).toBe('99');
    });

    it('searches deeply nested trees', () => {
      const tree = {
        children: [
          {
            children: [
              {
                children: [
                  { key: 'deep-key', element: { id: 77 } },
                ],
              },
            ],
          },
        ],
      };
      expect(findElementByKey(tree, 'deep-key')).toBe('77');
    });

    it('returns null when key is not found', () => {
      const tree = {
        children: [
          { key: 'exists', element: { id: 1 } },
        ],
      };
      expect(findElementByKey(tree, 'nonexistent')).toBeNull();
    });

    it('returns null for null input', () => {
      expect(findElementByKey(null, 'anything')).toBeNull();
    });

    it('prefers key match over tag match', () => {
      // If a node has both key and tag, key should match
      const tree = {
        children: [
          { key: 'my-key', tag: 'SomeTag', element: { id: 1 } },
          { key: 'other', tag: 'my-key', element: { id: 2 } },
        ],
      };
      // Should find the first node matching key='my-key'
      expect(findElementByKey(tree, 'my-key')).toBe('1');
    });
  });
});
