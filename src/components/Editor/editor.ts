import { SET_MEDIA_LIBRARY_VISIBILITY } from "@/store/modules/editor/mutation-types";
import Vue from "vue";
import { mapMutations, mapState } from "vuex";

export default Vue.extend({
  data() {
    return {};
  },

  computed: {
    ...mapState("editor", ["mediaLibraryVisible"]),
  },

  methods: {
    toggleMediaLibraryVisibility() {
      this.$store.commit(SET_MEDIA_LIBRARY_VISIBILITY, { newVisibility: !this.mediaLibraryVisible });
    },
  },
});
