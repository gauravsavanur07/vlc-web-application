import leafName from "@/utils/leaf-name";
import mediaLibrary from "./media-library";
import { SET_MEDIA_LIBRARY_VISIBILITY } from "./mutation-types";

const initialState = {
  mediaLibraryVisible: false,
};

const getters = {

};

const actions = {

};

const mutations = {
  [leafName(SET_MEDIA_LIBRARY_VISIBILITY)](state: any, payload: { newVisibility: boolean }) {
    state.mediaLibraryVisible = payload.newVisibility;
  },
};

export default {
  actions,
  getters,
  modules: {
    mediaLibrary,
  },
  mutations,
  namespaced: true,
  state: initialState,
};
