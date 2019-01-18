module.exports = function(grunt) {

    "use strict";

    grunt.initConfig({
        sass: {
            production: {
                files: {
                    "css-compiled/material.css": "sass/material.scss",
                    "css-compiled/material-wfont.css": "sass/material-wfont.scss",
                    "css-compiled/ripples.css": "sass/ripples.scss"
                }
            }
        },

        autoprefixer: {
            options: {
                browsers: ["last 3 versions", "ie 8", "ie 9", "ie 10", "ie 11"]
            },
            dist: {
                files: {
                    "css-compiled/material.css": "css-compiled/material.css",
                    "css-compiled/material-wfont.css": "css-compiled/material-wfont.css",
                    "css-compiled/ripples.css": "css-compiled/ripples.css"
                }
            },
        },

        cssmin: {
            minify: {
                expand: true,
                cwd: "css-compiled/",
                src: ["*.css", "!*.min.css"],
                dest: "css-compiled/",
                ext: ".min.css"
            }
        },

        copy: {
            css: {
                src: "css-compiled/*.min.css",
                dest: "template/material/"
            },
            js: {
                src: "scripts/*.js",
                dest: "template/material/"
            }

        }

    });
    grunt.loadNpmTasks("grunt-contrib-sass");
    grunt.loadNpmTasks("grunt-contrib-less");
    grunt.loadNpmTasks("grunt-autoprefixer");
    grunt.loadNpmTasks("grunt-contrib-cssmin");
    grunt.loadNpmTasks("grunt-contrib-copy");
    grunt.registerTask("default", ["sass", "autoprefixer", "cssmin", "copy"]);
};
