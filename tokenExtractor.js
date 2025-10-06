// @ts-check
/**
 * @type {import("@hediet/debug-visualizer-data-extraction").LoadDataExtractorsFn}
 */
module.exports = (register, helpers) => {
    register({
        id: "token-ast",
        getExtractions(data, collector, context) {
            if (!data || typeof data !== "object" || !("tlist" in data)) {
                return;
            }

            collector.addExtraction({
                id: "token-ast",
                name: "AST (Token)",
                priority: 1000,
                extractData() {
                    function toTree(token) {
                        return {
                            name: `${token.type}: ${token.data || ""}`,
                            children: (token.tlist || []).map(toTree)
                        };
                    }

                    return helpers.asData({
                        kind: { hierarchical: true },
                        root: toTree(data)
                    });
                },
            });
        },
    });
};
