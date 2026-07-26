import { defineConfig } from 'vitest/config'

export default defineConfig({
  test: {
    globals: true,
    environment: 'node',
    include: ['test/**/*.test.js'],
    exclude: ['node_modules/**'],
    coverage: {
      provider: 'v8',
      reporter: ['text', 'lcov', 'html'],
      include: [
        'lib/**/*.js',
        'dist-electron/*.cjs',
        'resources/tool_helper.py'
      ],
      exclude: ['node_modules/**', 'test/**', 'dist/**']
    },
    testTimeout: 30000,
    hookTimeout: 30000,
    deps: {
      optimizer: {
        ssr: {
          include: ['lib/**']
        }
      }
    }
  }
})
