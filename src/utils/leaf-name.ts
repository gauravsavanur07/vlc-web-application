export default (namespaced: string): string => {
  const i = namespaced.lastIndexOf("/");

  return (i === -1 ? namespaced : namespaced.substr(i + 1));
};
