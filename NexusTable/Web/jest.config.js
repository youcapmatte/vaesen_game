/** Unit tests for the pure logic in modules/ and common/ (no Next runtime). */
module.exports = {
  preset: 'ts-jest',
  testEnvironment: 'node',
  moduleNameMapper: { '^@/(.*)$': '<rootDir>/$1' },
  transform: { '^.+\.tsx?$': ['ts-jest', { tsconfig: { jsx: 'react-jsx' } }] },
  testPathIgnorePatterns: ['/node_modules/', '/.next/'],
};
