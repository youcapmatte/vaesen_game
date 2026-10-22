import { createTheme, type MantineColorsTuple } from "@mantine/core";

/**
 * The look of the Vaesen rulebook: umber night pages, parchment, sepia ink, dried-blood red and tarnished gold.
 * Colours are also CSS variables (--nx-*, see global.css) for the hand-drawn parts.
 */
const blood: MantineColorsTuple = [
  "#fbecea", "#f0d2ce", "#dfa49c", "#ce7468", "#bf4c3e", "#b13628", "#a02b1f", "#8e2a22", "#74221b", "#5a1a14",
];

const gold: MantineColorsTuple = [
  "#fbf5e9", "#f0e6cf", "#e2cfa3", "#d4b775", "#c8a350", "#c0963c", "#b8955a", "#9b7a3e", "#7d6131", "#5e4824",
];

const umber: MantineColorsTuple = [
  "#e8dcc2", "#cfc2a6", "#a8997c", "#857760", "#665a48", "#4b4135", "#383027", "#2d251d", "#221c16", "#14110e",
];

const moss: MantineColorsTuple = [
  "#eef2ea", "#d9e0d2", "#b4c2a8", "#8fa47e", "#71895d", "#5e784a", "#4a5a43", "#3e4c38", "#323e2d", "#252e21",
];

export const theme = createTheme({
  primaryColor: "blood",
  primaryShade: 7,
  colors: { blood, gold, umber, moss, dark: umber },
  black: "#14110e",
  white: "#e8dcc2",
  fontFamily: "var(--font-body), Georgia, 'Times New Roman', serif",
  fontFamilyMonospace: "ui-monospace, Consolas, monospace",
  headings: {
    fontFamily: "var(--font-display), Georgia, serif",
    fontWeight: "400",
    sizes: {
      h1: { fontSize: "2.75rem", lineHeight: "1.1" },
      h2: { fontSize: "2rem", lineHeight: "1.15" },
      h3: { fontSize: "1.5rem", lineHeight: "1.2" },
      h4: { fontSize: "1.2rem", lineHeight: "1.25" },
    },
  },
  fontSizes: { xs: "0.85rem", sm: "0.95rem", md: "1.05rem", lg: "1.2rem", xl: "1.4rem" },
  defaultRadius: "xs",
  cursorType: "pointer",
  components: {
    Button: {
      defaultProps: { variant: "filled" },
      styles: {
        root: { fontFamily: "var(--font-display)", fontWeight: 400, letterSpacing: "0.06em", fontSize: "1.05rem" },
      },
    },
    Paper: {
      defaultProps: { radius: "xs" },
    },
    Tooltip: {
      defaultProps: { color: "umber.8", withArrow: true },
    },
    Badge: {
      styles: { root: { fontFamily: "var(--font-display)", fontWeight: 400, letterSpacing: "0.05em", textTransform: "none" } },
    },
    Modal: {
      defaultProps: { centered: true, overlayProps: { backgroundOpacity: 0.7, blur: 2 } },
    },
  },
});
