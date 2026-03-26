#include "Huffman.hpp"
#include <algorithm>
#include <queue>

// Walk the tree and record the depth of each leaf
void Huffman::extractLengths(const std::vector<Node>& nodes, int idx,
                             uint8_t depth, std::vector<uint8_t>& lengths)
{
    const Node& n = nodes[idx];
    if (n.symbol >= 0)
    {
        lengths[n.symbol] = depth;
        return;
    }
    extractLengths(nodes, n.left,  depth + 1, lengths);
    extractLengths(nodes, n.right, depth + 1, lengths);
}

// Kraft-inequality based clamping (package-merge simplified)
void Huffman::clampLengths(std::vector<uint8_t>& lengths, uint8_t maxBits)
{
    // Collect nonzero lengths
    bool changed = true;
    while (changed)
    {
        changed = false;
        // Find the longest code
        uint8_t longest = *std::max_element(lengths.begin(), lengths.end());
        if (longest <= maxBits)
            break;
        // Reduce all codes that exceed maxBits
        for (auto& l : lengths)
        {
            if (l > maxBits)
            {
                l = maxBits;
                changed = true;
            }
        }
        // Now rebalance using the Kraft inequality
        // Sum of 2^(maxBits - l_i) must equal 2^maxBits
        uint32_t kraft = 0;
        for (auto l : lengths)
        {
            if (l > 0)
                kraft += (1u << (maxBits - l));
        }
        uint32_t target = (1u << maxBits);
        if (kraft > target)
        {
            // Need to increase some code lengths
            // Increase shortest codes first
            std::vector<std::pair<uint8_t, size_t>> sorted; // length, index
            for (size_t i = 0; i < lengths.size(); i++)
            {
                if (lengths[i] > 0)
                    sorted.push_back({lengths[i], i});
            }
            std::sort(sorted.begin(), sorted.end());

            while (kraft > target)
            {
                for (auto& p : sorted)
                {
                    if (lengths[p.second] < maxBits)
                    {
                        uint32_t reduction = (1u << (maxBits - lengths[p.second])) -
                                             (1u << (maxBits - lengths[p.second] - 1));
                        lengths[p.second]++;
                        kraft -= reduction;
                        if (kraft <= target)
                            break;
                    }
                }
                // If we can't reduce further, break
                bool canReduce = false;
                for (auto& p : sorted)
                {
                    if (lengths[p.second] < maxBits)
                        canReduce = true;
                }
                if (!canReduce)
                    break;
            }
        }
        else if (kraft < target)
        {
            // Need to decrease some code lengths — decrease longest first
            std::vector<std::pair<uint8_t, size_t>> sorted;
            for (size_t i = 0; i < lengths.size(); i++)
            {
                if (lengths[i] > 0)
                    sorted.push_back({lengths[i], i});
            }
            std::sort(sorted.begin(), sorted.end(), std::greater<>());

            while (kraft < target)
            {
                for (auto& p : sorted)
                {
                    if (lengths[p.second] > 1)
                    {
                        uint32_t gain = (1u << (maxBits - lengths[p.second] + 1)) -
                                        (1u << (maxBits - lengths[p.second]));
                        if (kraft + gain <= target)
                        {
                            lengths[p.second]--;
                            kraft += gain;
                            if (kraft == target)
                                break;
                        }
                    }
                }
                break; // one pass is enough
            }
        }
        changed = false; // exit after rebalance
    }
}

// Build optimal lengths from frequencies
std::vector<uint8_t> Huffman::buildCodeLengths(const std::vector<uint32_t>& freqs,
                                                uint8_t maxBits)
{
    int n = (int)freqs.size();
    std::vector<uint8_t> lengths(n, 0);

    // Count symbols with nonzero frequency
    std::vector<int> active;
    for (int i = 0; i < n; i++)
    {
        if (freqs[i] > 0)
            active.push_back(i);
    }

    if (active.empty())
        return lengths;

    if (active.size() == 1)
    {
        lengths[active[0]] = 1;
        return lengths;
    }

    // Build Huffman tree using a min-heap
    std::vector<Node> nodes;
    nodes.reserve(active.size() * 2);

    // Comparator: lower frequency = higher priority
    auto cmp = [&](int a, int b) { return nodes[a].freq > nodes[b].freq; };
    std::priority_queue<int, std::vector<int>, decltype(cmp)> pq(cmp);

    for (int sym : active)
    {
        int idx = (int)nodes.size();
        nodes.push_back({freqs[sym], sym, -1, -1});
        pq.push(idx);
    }

    while (pq.size() > 1)
    {
        int left = pq.top(); pq.pop();
        int right = pq.top(); pq.pop();

        int idx = (int)nodes.size();
        nodes.push_back({nodes[left].freq + nodes[right].freq, -1, left, right});
        pq.push(idx);
    }

    int root = pq.top();
    extractLengths(nodes, root, 0, lengths);

    // If only two symbols, both get depth 1 naturally.
    // But if the tree is degenerate I might exceed maxBits:
    clampLengths(lengths, maxBits);

    return lengths;
}

// Given code lengths, build canonical Huffman codes (RFC 1951 §3.2.2)
HuffmanTable Huffman::buildCanonicalTable(const std::vector<uint8_t>& lengths)
{
    int n = (int)lengths.size();
    HuffmanTable table;
    table.lengths = lengths;
    table.codes.resize(n, 0);

    if (n == 0)
        return table;

    uint8_t maxLen = *std::max_element(lengths.begin(), lengths.end());
    if (maxLen == 0)
        return table;

    // Count the number of codes for each code length
    std::vector<int> blCount(maxLen + 1, 0);
    for (int i = 0; i < n; i++)
    {
        if (lengths[i] > 0)
            blCount[lengths[i]]++;
    }

    // Find the numerical value of the smallest code for each code length
    std::vector<uint16_t> nextCode(maxLen + 1, 0);
    uint16_t code = 0;
    for (int bits = 1; bits <= maxLen; bits++)
    {
        code = (code + blCount[bits - 1]) << 1;
        nextCode[bits] = code;
    }

    // Assign numerical values to all codes
    for (int i = 0; i < n; i++)
    {
        if (lengths[i] != 0)
        {
            table.codes[i] = nextCode[lengths[i]];
            nextCode[lengths[i]]++;
        }
    }

    return table;
}