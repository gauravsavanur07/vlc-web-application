import Editor from "@/components/Editor/Editor";
import Vue from "vue";
import Router from "vue-router";

Vue.use(Router);

export default new Router({
  routes: [
    {
      component: Editor,
      name: "Editor",
      path: "/",
    },
  ],
});
