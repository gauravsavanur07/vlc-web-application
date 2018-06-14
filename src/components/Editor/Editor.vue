<script lang="ts" src="./editor.ts"></script>
<style scoped lang="scss" src="./editor.scss"></style>

<template>
  <div>
    <button @click="toggleMediaLibraryVisibility"></button>
    <span>{{ String(mediaLibraryVisible) }}</span>
  </div>
</template>
