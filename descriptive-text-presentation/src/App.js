import React, { useState } from 'react';
import './App.css';

function App() {
  const [currentSlide, setCurrentSlide] = useState(0);

  const slides = [
    {
      type: 'title',
      title: 'Descriptive Text',
      subtitle: 'A Comprehensive Guide',
      supervisor: 'Supervised by Professor A.EL MAATOUKI',
      editor: 'Edited by Douae Sadok'
    },
    {
      type: 'team',
      title: 'Group Members & Their Roles',
      members: [
        { name: 'Hiba Hadije', role: 'Definition and Purpose of Descriptive Text' },
        { name: 'Sabrine Hliya', role: 'Structure of Descriptive Text (Identification + Detailed Description)' },
        { name: 'Fatima Ezzahra Khouya', role: 'Types of Descriptive Text (Person, Place, Object, Animal, Event)' },
        { name: 'Fatima Ezzahra Khouya & Sabrine Hliya', role: 'Language Features (Adjectives, Tenses, Linking Verbs, Sensory Words)' },
        { name: 'Douae Sadok', role: 'Importance of Descriptive Writing & Conclusion' }
      ]
    },
    {
      type: 'content',
      title: 'Definition',
      icon: '🟡',
      content: [
        {
          heading: 'What is Descriptive Text?',
          text: 'A descriptive text is a kind of writing that describes a person, place, object, animal, or event in detail. Its goal is to make the reader see, hear, and feel what the writer is describing.'
        }
      ]
    },
    {
      type: 'content',
      title: 'Purpose',
      icon: '🟡',
      content: [
        {
          heading: 'Why Use Descriptive Text?',
          text: 'The purpose of a descriptive text is to create a clear picture in the reader\'s mind and help them imagine the subject as if they were seeing it themselves.'
        }
      ]
    },
    {
      type: 'structure',
      title: 'Structure of Descriptive Text',
      icon: '🔴',
      structure: [
        { step: '1. Introduction', desc: 'Introduction to the topic or character being described.' },
        { step: '2. General Description', desc: 'General description of the topic or character, including general attributes and characteristics.' },
        { step: '3. Details', desc: 'Description of specific details about the topic or character, such as physical or behavioral traits.' },
        { step: '4. Conclusion', desc: 'Summary of the description or a comment on the topic or character.' }
      ],
      example: {
        title: 'Example: Marrakech',
        steps: [
          { label: 'Introduction', text: 'Marrakech is one of the most beautiful cities in Morocco.' },
          { label: 'General Description', text: 'Marrakech is a historic city, known for its authentic Arab architecture and hot climate.' },
          { label: 'Details', text: 'Its narrow streets, bustling markets, and beautiful gardens make Marrakech a unique city.' },
          { label: 'Conclusion', text: 'Marrakech is a city worth visiting, where visitors can enjoy its beauty and history.' }
        ]
      }
    },
    {
      type: 'types',
      title: 'Types of Descriptive Text',
      icon: '🔴',
      types: [
        { type: 'Person', example: 'My friend is tall, and his smile is beautiful.' },
        { type: 'Place', example: 'The garden is spacious and full of colorful flowers.' },
        { type: 'Object', example: 'The book is large, and its cover is red.' },
        { type: 'Animal', example: 'The cat is cute, and its eyes are green.' },
        { type: 'Event', example: 'The party was fun, and everyone was dancing and singing.' }
      ]
    },
    {
      type: 'features',
      title: 'Language Features',
      icon: '🔴',
      features: [
        { feature: 'Adjectives', example: 'beautiful, tall, soft' },
        { feature: 'Tenses', example: 'The garden was quiet / The sun is rising' },
        { feature: 'Linking Verbs', example: 'He seems happy / The weather was mild' },
        { feature: 'Sensory Words', example: 'The flowers smell sweet / The sound is quiet / The fabric feels soft' }
      ]
    },
    {
      type: 'characteristics',
      title: 'Key Characteristics',
      icon: '🔴',
      characteristics: [
        { title: 'Clarity', desc: 'Descriptive text should be clear and easy to understand, allowing the reader to visualize the scene or character.' },
        { title: 'Accuracy', desc: 'Descriptive text should be accurate in its details, enabling the reader to form a clear picture.' },
        { title: 'Vividness', desc: 'Descriptive text should be vivid and engaging, capturing the reader\'s attention and making them want to know more.' },
        { title: 'Effective Word Choice', desc: 'Words should be used effectively to describe the scene or character, avoiding excessive description.' },
        { title: 'Focus on Important Details', desc: 'Focus on the details that matter, helping the reader understand the scene or character.' }
      ],
      examples: [
        'Using descriptive adjectives like "beautiful", "strong", "fast"',
        'Using descriptive verbs like "runs", "jumps", "smiles"',
        'Using setting and time to describe the scene',
        'Using sensory details like "smell of flowers", "sound of birds"'
      ]
    },
    {
      type: 'importance',
      title: 'Importance of Descriptive Writing',
      icon: '🔴',
      content: 'Descriptive writing is important because it brings ideas to life and helps readers clearly imagine what the writer is describing. By using vivid details and sensory language, it creates strong images, adds emotion, and makes the writing more engaging. It also improves storytelling and communication by making scenes, characters, and concepts easier to understand and more memorable.'
    },
    {
      type: 'conclusion',
      title: 'Conclusion',
      icon: '🔴',
      content: 'In conclusion, descriptive writing plays an essential role in helping readers visualize and understand the subject being described. By using clear structure, vivid language, and sensory details, a descriptive text can create strong images and make the information more memorable. Whether describing a person, place, object, animal, or event, effective descriptive writing allows the reader to feel closer to the scene and fully experience it through words.'
    },
    {
      type: 'thankyou',
      title: 'Thank You!',
      subtitle: 'Questions?'
    }
  ];

  const nextSlide = () => {
    if (currentSlide < slides.length - 1) {
      setCurrentSlide(currentSlide + 1);
    }
  };

  const prevSlide = () => {
    if (currentSlide > 0) {
      setCurrentSlide(currentSlide - 1);
    }
  };

  const goToSlide = (index) => {
    setCurrentSlide(index);
  };

  const slide = slides[currentSlide];

  return (
    <div className="min-h-screen bg-gradient-to-br from-blue-50 via-purple-50 to-pink-50 flex items-center justify-center p-4">
      <div className="w-full max-w-6xl">
        <div className="bg-white rounded-2xl shadow-2xl overflow-hidden min-h-[600px] flex flex-col">
          <div className="flex-1 p-12">
            {slide.type === 'title' && (
              <div className="h-full flex flex-col items-center justify-center text-center space-y-8">
                <div className="space-y-4">
                  <h1 className="text-7xl font-bold bg-gradient-to-r from-blue-600 via-purple-600 to-pink-600 bg-clip-text text-transparent">
                    {slide.title}
                  </h1>
                  <p className="text-3xl text-gray-600 font-light">{slide.subtitle}</p>
                </div>
                <div className="space-y-2 mt-12">
                  <p className="text-lg text-gray-500">{slide.supervisor}</p>
                  <p className="text-lg text-gray-500">{slide.editor}</p>
                </div>
              </div>
            )}

            {slide.type === 'team' && (
              <div className="h-full flex flex-col">
                <h2 className="text-5xl font-bold text-gray-800 mb-8 text-center">{slide.title}</h2>
                <div className="space-y-6 flex-1">
                  {slide.members.map((member, idx) => (
                    <div key={idx} className="bg-gradient-to-r from-blue-50 to-purple-50 rounded-xl p-6 border-l-4 border-blue-500 hover:shadow-lg transition-shadow">
                      <div className="flex items-start space-x-4">
                        <div className="flex-shrink-0 w-10 h-10 bg-blue-500 text-white rounded-full flex items-center justify-center font-bold text-lg">
                          {idx + 1}
                        </div>
                        <div className="flex-1">
                          <h3 className="text-xl font-bold text-gray-800 mb-2">{member.name}</h3>
                          <p className="text-gray-600">{member.role}</p>
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {slide.type === 'content' && (
              <div className="h-full flex flex-col justify-center">
                <h2 className="text-5xl font-bold text-gray-800 mb-8 flex items-center">
                  <span className="mr-4 text-6xl">{slide.icon}</span>
                  {slide.title}
                </h2>
                <div className="space-y-6">
                  {slide.content.map((item, idx) => (
                    <div key={idx} className="bg-gradient-to-r from-yellow-50 to-orange-50 rounded-xl p-8 border-l-4 border-yellow-500">
                      <h3 className="text-2xl font-bold text-gray-800 mb-4">{item.heading}</h3>
                      <p className="text-xl text-gray-700 leading-relaxed">{item.text}</p>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {slide.type === 'structure' && (
              <div className="h-full flex flex-col">
                <h2 className="text-5xl font-bold text-gray-800 mb-6 flex items-center">
                  <span className="mr-4 text-6xl">{slide.icon}</span>
                  {slide.title}
                </h2>
                <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 flex-1">
                  <div className="space-y-4">
                    {slide.structure.map((item, idx) => (
                      <div key={idx} className="bg-gradient-to-r from-red-50 to-pink-50 rounded-xl p-5 border-l-4 border-red-500">
                        <h3 className="text-lg font-bold text-gray-800 mb-2">{item.step}</h3>
                        <p className="text-gray-600">{item.desc}</p>
                      </div>
                    ))}
                  </div>
                  <div className="bg-gradient-to-br from-blue-50 to-indigo-50 rounded-xl p-6 border-2 border-blue-300">
                    <h3 className="text-2xl font-bold text-gray-800 mb-4">{slide.example.title}</h3>
                    <div className="space-y-3">
                      {slide.example.steps.map((step, idx) => (
                        <div key={idx} className="bg-white rounded-lg p-4 shadow-sm">
                          <p className="text-sm font-semibold text-blue-600 mb-1">{step.label}</p>
                          <p className="text-gray-700">{step.text}</p>
                        </div>
                      ))}
                    </div>
                  </div>
                </div>
              </div>
            )}

            {slide.type === 'types' && (
              <div className="h-full flex flex-col">
                <h2 className="text-5xl font-bold text-gray-800 mb-8 flex items-center">
                  <span className="mr-4 text-6xl">{slide.icon}</span>
                  {slide.title}
                </h2>
                <div className="grid grid-cols-1 md:grid-cols-2 gap-6 flex-1">
                  {slide.types.map((item, idx) => (
                    <div key={idx} className="bg-gradient-to-br from-purple-50 to-pink-50 rounded-xl p-6 border-l-4 border-purple-500 hover:shadow-lg transition-shadow">
                      <h3 className="text-2xl font-bold text-gray-800 mb-3 flex items-center">
                        <span className="mr-2">→</span>
                        {item.type}
                      </h3>
                      <p className="text-lg text-gray-700 italic">"{item.example}"</p>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {slide.type === 'features' && (
              <div className="h-full flex flex-col">
                <h2 className="text-5xl font-bold text-gray-800 mb-8 flex items-center">
                  <span className="mr-4 text-6xl">{slide.icon}</span>
                  {slide.title}
                </h2>
                <div className="grid grid-cols-1 md:grid-cols-2 gap-6 flex-1">
                  {slide.features.map((item, idx) => (
                    <div key={idx} className="bg-gradient-to-br from-green-50 to-teal-50 rounded-xl p-6 border-l-4 border-green-500 hover:shadow-lg transition-shadow">
                      <h3 className="text-2xl font-bold text-gray-800 mb-3 flex items-center">
                        <span className="mr-2">→</span>
                        {item.feature}
                      </h3>
                      <p className="text-lg text-gray-700">
                        <span className="font-semibold">Example:</span> {item.example}
                      </p>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {slide.type === 'characteristics' && (
              <div className="h-full flex flex-col">
                <h2 className="text-5xl font-bold text-gray-800 mb-6 flex items-center">
                  <span className="mr-4 text-6xl">{slide.icon}</span>
                  {slide.title}
                </h2>
                <div className="space-y-4 flex-1 overflow-y-auto">
                  {slide.characteristics.map((item, idx) => (
                    <div key={idx} className="bg-gradient-to-r from-indigo-50 to-blue-50 rounded-xl p-5 border-l-4 border-indigo-500">
                      <h3 className="text-xl font-bold text-gray-800 mb-2">{item.title}</h3>
                      <p className="text-gray-700">{item.desc}</p>
                    </div>
                  ))}
                  <div className="bg-gradient-to-r from-yellow-50 to-amber-50 rounded-xl p-5 border-l-4 border-yellow-500 mt-4">
                    <h3 className="text-xl font-bold text-gray-800 mb-3">Examples:</h3>
                    <ul className="space-y-2">
                      {slide.examples.map((example, idx) => (
                        <li key={idx} className="text-gray-700 flex items-start">
                          <span className="mr-2 text-yellow-600">•</span>
                          {example}
                        </li>
                      ))}
                    </ul>
                  </div>
                </div>
              </div>
            )}

            {slide.type === 'importance' && (
              <div className="h-full flex flex-col justify-center">
                <h2 className="text-5xl font-bold text-gray-800 mb-8 flex items-center">
                  <span className="mr-4 text-6xl">{slide.icon}</span>
                  {slide.title}
                </h2>
                <div className="bg-gradient-to-br from-orange-50 to-red-50 rounded-xl p-8 border-l-4 border-orange-500">
                  <p className="text-xl text-gray-700 leading-relaxed">{slide.content}</p>
                </div>
              </div>
            )}

            {slide.type === 'conclusion' && (
              <div className="h-full flex flex-col justify-center">
                <h2 className="text-5xl font-bold text-gray-800 mb-8 flex items-center">
                  <span className="mr-4 text-6xl">{slide.icon}</span>
                  {slide.title}
                </h2>
                <div className="bg-gradient-to-br from-purple-50 to-indigo-50 rounded-xl p-8 border-l-4 border-purple-500">
                  <p className="text-xl text-gray-700 leading-relaxed">{slide.content}</p>
                </div>
              </div>
            )}

            {slide.type === 'thankyou' && (
              <div className="h-full flex flex-col items-center justify-center text-center space-y-8">
                <h1 className="text-8xl font-bold bg-gradient-to-r from-green-600 via-blue-600 to-purple-600 bg-clip-text text-transparent">
                  {slide.title}
                </h1>
                <p className="text-4xl text-gray-600 font-light">{slide.subtitle}</p>
                <div className="text-6xl mt-8">🎓</div>
              </div>
            )}
          </div>

          <div className="bg-gray-100 px-12 py-6 flex items-center justify-between border-t-2 border-gray-200">
            <button
              onClick={prevSlide}
              disabled={currentSlide === 0}
              className={`px-6 py-3 rounded-lg font-semibold transition-all ${
                currentSlide === 0
                  ? 'bg-gray-300 text-gray-500 cursor-not-allowed'
                  : 'bg-blue-500 text-white hover:bg-blue-600 shadow-md hover:shadow-lg'
              }`}
            >
              ← Previous
            </button>

            <div className="flex items-center space-x-2">
              {slides.map((_, idx) => (
                <button
                  key={idx}
                  onClick={() => goToSlide(idx)}
                  className={`w-3 h-3 rounded-full transition-all ${
                    idx === currentSlide
                      ? 'bg-blue-500 w-8'
                      : 'bg-gray-300 hover:bg-gray-400'
                  }`}
                  aria-label={`Go to slide ${idx + 1}`}
                />
              ))}
            </div>

            <button
              onClick={nextSlide}
              disabled={currentSlide === slides.length - 1}
              className={`px-6 py-3 rounded-lg font-semibold transition-all ${
                currentSlide === slides.length - 1
                  ? 'bg-gray-300 text-gray-500 cursor-not-allowed'
                  : 'bg-blue-500 text-white hover:bg-blue-600 shadow-md hover:shadow-lg'
              }`}
            >
              Next →
            </button>
          </div>

          <div className="bg-gray-50 px-12 py-3 text-center text-sm text-gray-500 border-t border-gray-200">
            Slide {currentSlide + 1} of {slides.length}
          </div>
        </div>

        <div className="mt-6 text-center text-gray-600">
          <p className="text-sm">Use arrow keys ← → or click buttons to navigate</p>
        </div>
      </div>
    </div>
  );
}

export default App;
